#include <iostream>
#include <chrono>
#include <ctime>
#include "srrg_hbst_types/binary_tree.hpp"

#if CV_MAJOR_VERSION == 2
#elif CV_MAJOR_VERSION == 3
  #include <opencv2/xfeatures2d.hpp>
#else
  #error OpenCV version not supported
#endif

//ds feature handling
cv::Ptr<cv::FastFeatureDetector> keypoint_detector;
cv::Ptr<cv::DescriptorExtractor> descriptor_extractor;

//ds visualization buffers
std::vector<cv::Mat> images(10);

//ds buffer keypoints globally - otherwise the memory pointers might get broken by the scope
std::vector<std::vector<cv::KeyPoint>> buffer_keypoints(10);

//ds retrieves HBST matchables from an opencv image stored at the provided location
const srrg_hbst::BinaryTree256::MatchableVector getMatchables(const std::string& filename_image_, const uint64_t& identifier_tree_);

int32_t main(int32_t argc_, char** argv_) {

  //ds validate input
  if (argc_ != 2) {
    std::cerr << "invalid call - please use: ./match_incremental_opencv_pointers /path/to/srrg_hbst/examples/test_images" << std::endl;
    return 0;
  }

  //ds feature handling
#if CV_MAJOR_VERSION == 2
  keypoint_detector    = new cv::FastFeatureDetector();
  descriptor_extractor = new cv::BriefDescriptorExtractor(32);
#elif CV_MAJOR_VERSION == 3
  keypoint_detector    = cv::FastFeatureDetector::create();
  descriptor_extractor = cv::xfeatures2d::BriefDescriptorExtractor::create(32);
#endif

  //ds measurements
  std::chrono::time_point<std::chrono::system_clock> time_begin;
  std::chrono::duration<double> duration_construction(0);
  std::chrono::duration<double> duration_query(0);
  double matching_ratio = 0;
  const uint32_t maximum_matching_distance = 25;

  //ds get test image path
  const std::string test_images_folder = argv_[1];

  //ds number of test images
  const uint32_t number_of_images = 10;

  //ds fill matchables vector for each image
  std::vector<srrg_hbst::BinaryTree256::MatchableVector> matchables_per_image(number_of_images);
  for (uint32_t u = 0; u < number_of_images; ++u) {

    //ds load matchables
    char buffer[32];
    std::sprintf(buffer, "image_%02u.pgm", u);
    const std::string file_test_image = test_images_folder + "/" + buffer;
    matchables_per_image[u] = getMatchables(file_test_image, u);
  }

  //ds create HBST for first image
  std::printf("building tree with image [%02u]\n", 0);
  time_begin = std::chrono::system_clock::now();
  std::shared_ptr<srrg_hbst::BinaryTree256> hbst_tree = std::make_shared<srrg_hbst::BinaryTree256>(0, matchables_per_image[0]);
  duration_construction += std::chrono::system_clock::now()-time_begin;

  //ds train tree on all subsequent images
  for (uint32_t u = 1; u < number_of_images; ++u) {
    std::printf("training tree with image [%02u]\n", u);
    time_begin = std::chrono::system_clock::now();
    hbst_tree->add(matchables_per_image[u]);
    duration_construction += std::chrono::system_clock::now()-time_begin;
  }

  //ds check each image against each other and itself (100% ratio)
  std::printf("starting matching with maximum distance: %u \n", maximum_matching_distance);
  for (uint32_t index_query = 0; index_query < number_of_images; ++index_query) {

    //ds query HBST with image 1
    srrg_hbst::BinaryTree256::MatchVectorMap matches;
    time_begin = std::chrono::system_clock::now();
    hbst_tree->match(matchables_per_image[index_query], matches, maximum_matching_distance);
    duration_query += std::chrono::system_clock::now()-time_begin;

    //ds check all match vectors in the map
    for (uint32_t index_reference = 0; index_reference < number_of_images; ++index_reference) {
      std::printf("matches for QUERY [%02u] to REFERENCE [%02u]: %4lu (matching ratio: %5.3f)\n", index_query, index_reference, matches.at(index_reference).size(),
                  static_cast<double>(matches.at(index_reference).size())/matchables_per_image[index_reference].size());
      matching_ratio += static_cast<double>(matches.at(index_reference).size())/matchables_per_image[index_reference].size();

      //ds show the matching in an image pair
      cv::Mat image_display;
      cv::vconcat(images[index_query], images[index_reference], image_display);
      cv::cvtColor(image_display, image_display, CV_GRAY2RGB);

      //ds shift to lower image
      cv::Point2f shift(0, images[index_query].rows);

      //ds draw correspondences
      for (const srrg_hbst::BinaryTree256::Match& match: matches.at(index_reference)) {

        //ds directly get the keypoint objects
        const cv::KeyPoint keypoint_query     = *(reinterpret_cast<const cv::KeyPoint*>(match.pointer_query));
        const cv::KeyPoint keypoint_reference = *(reinterpret_cast<const cv::KeyPoint*>(match.pointer_reference));

        //ds draw correspondence
        cv::line(image_display, keypoint_query.pt, keypoint_reference.pt+shift, cv::Scalar(0, 255, 0));

        //ds draw reference point in upper image
        cv::circle(image_display, keypoint_query.pt, 2, cv::Scalar(255, 0, 0));

        //ds draw query point in lower image
        cv::circle(image_display, keypoint_reference.pt+shift, 2, cv::Scalar(0, 0, 255));
      }
      cv::imshow("matching (top: QUERY, bot: REFERENCE)", image_display);
      cv::waitKey(0);
    }
  }

  //ds statistics summary
  std::cerr << "------------------------------------------------" << std::endl;
  std::printf("   construction duration (s): %6.4f\n", duration_construction.count());
  std::printf("  average query duration (s): %6.4f\n", duration_query.count()/number_of_images);
  std::printf("average query matching ratio: %6.4f\n", matching_ratio/(number_of_images*number_of_images));
  std::cerr << "------------------------------------------------" << std::endl;
  return 0;
}

const srrg_hbst::BinaryTree256::MatchableVector getMatchables(const std::string& filename_image_, const uint64_t& identifier_tree_) {

  //ds allocate empty matchable vector
  srrg_hbst::BinaryTree256::MatchableVector matchables(0);

  //ds load image (project root folder)
  images[identifier_tree_] = cv::imread(filename_image_, CV_LOAD_IMAGE_GRAYSCALE);

  //ds detect FAST keypoints
  keypoint_detector->detect(images[identifier_tree_], buffer_keypoints[identifier_tree_]);

  //ds compute BRIEF descriptors
  cv::Mat descriptors;
  descriptor_extractor->compute(images[identifier_tree_], buffer_keypoints[identifier_tree_], descriptors);
  std::cerr << "loaded image: " << filename_image_ << " with keypoints/descriptors: " << descriptors.rows << std::endl;

  //ds get descriptors to HBST format
  return srrg_hbst::BinaryTree256::getMatchablesWithPointer<cv::KeyPoint>(descriptors, buffer_keypoints[identifier_tree_], identifier_tree_);
}