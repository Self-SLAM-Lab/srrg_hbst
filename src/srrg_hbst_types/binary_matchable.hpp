#pragma once
#include <stdint.h>
#include <bitset>
#include <vector>
#include <map>
#include <assert.h>

//ds if opencv is present on building system
#ifdef SRRG_HBST_HAS_OPENCV
#include <opencv2/core/version.hpp>
#include <opencv2/opencv.hpp>
#endif

namespace srrg_hbst {

//! @class default matching object (wraps the input descriptors and more)
//! @param descriptor_size_bits_ number of bits for the native descriptor
template<uint32_t descriptor_size_bits_ = 256>
class BinaryMatchable {

//ds template forwarding
public:

  //! @brief descriptor type (extended by augmented bits, no effect if zero)
  typedef std::bitset<descriptor_size_bits_> Descriptor;

//ds shared properties
public:

  //! @brief descriptor size in bits (for all matchables)
  static constexpr uint32_t descriptor_size_bits = descriptor_size_bits_;

  //! @brief descriptor size in bytes (for all matchables, size after the number of augmented bits)
  static constexpr uint32_t raw_descriptor_size_bytes = descriptor_size_bits_/8;

  //! @brief descriptor size, bits in whole bytes (corresponds to descriptor_size_bits for a byte-wise descriptor)
  static constexpr uint32_t descriptor_size_bits_in_bytes = (descriptor_size_bits_/8)*8;

  //! @brief overflowing single bits (normally 0)
  static constexpr uint32_t descriptor_size_bits_overflow = descriptor_size_bits_-descriptor_size_bits_in_bytes;

//ds ctor/dtor
public:

  //! @brief default constructor: DISABLED
  BinaryMatchable() = delete;

  //! @brief constructor with index for association
  //! @param[in] identifier_ association index
  //! @param[in] descriptor_ HBST descriptor
  //! @param[in] identifier_tree_ HBST tree identifier (optional)
  BinaryMatchable(const uint64_t& identifier_,
                  const Descriptor& descriptor_,
                  const uint64_t& identifier_image_ = 0): descriptor(descriptor_),
                                                          _identifier_image(identifier_image_),
                                                          _identifier(identifier_),
                                                          _pointer(nullptr) {
    identifiers_image.push_back(_identifier_image);
    identifiers.insert(std::make_pair(_identifier_image, _identifier));
    pointers.insert(std::make_pair(_identifier_image, _pointer));
  }

  //! @brief constructor with an object pointer for association
  //! @param[in] pointer_ associated object
  //! @param[in] descriptor_ HBST descriptor
  //! @param[in] identifier_tree_ HBST tree identifier (optional)
  BinaryMatchable(void* pointer_,
                  const Descriptor& descriptor_,
                  const uint64_t& identifier_image_ = 0): descriptor(descriptor_),
                                                          _identifier_image(identifier_image_),
                                                          _identifier(0),
                                                          _pointer(pointer_) {
    identifiers_image.push_back(_identifier_image);
    identifiers.insert(std::make_pair(_identifier_image, _identifier));
    pointers.insert(std::make_pair(_identifier_image, _pointer));
  }

  //! @brief constructor with index and object pointer for association
  //! @param[in] identifier_ association index
  //! @param[in] pointer_ associated object
  //! @param[in] descriptor_ HBST descriptor
  //! @param[in] identifier_tree_ HBST tree identifier (optional)
  //! @param[in] augmentation_ HBST augmentation vector (optional)
  BinaryMatchable(const uint64_t& identifier_,
                  void* pointer_,
                  const Descriptor& descriptor_,
                  const uint64_t& identifier_image_ = 0): descriptor(descriptor_),
                                                          _identifier_image(identifier_image_),
                                                          _identifier(identifier_),
                                                          _pointer(pointer_) {
    identifiers_image.push_back(_identifier_image);
    identifiers.insert(std::make_pair(_identifier_image, _identifier));
    pointers.insert(std::make_pair(_identifier_image, _pointer));
  }

//ds wrapped constructors - only available if OpenCV is present on building system
#ifdef SRRG_HBST_HAS_OPENCV
  BinaryMatchable(const uint64_t& identifier_,
                  const cv::Mat& descriptor_,
                  const uint64_t& identifier_image_ = 0): BinaryMatchable(identifier_,
                                                          getDescriptor(descriptor_),
                                                          identifier_image_) {}

  BinaryMatchable(void* pointer_,
                  const cv::Mat& descriptor_,
                  const uint64_t& identifier_image_ = 0): BinaryMatchable(pointer_,
                                                          getDescriptor(descriptor_),
                                                          identifier_image_) {}

  BinaryMatchable(const uint64_t& identifier_,
                  void* pointer_,
                  const cv::Mat& descriptor_,
                  const uint64_t& identifier_image_ = 0): BinaryMatchable(identifier_,
                                                                          pointer_,
                                                                          getDescriptor(descriptor_),
                                                                          identifier_image_) {}
#endif

  //! @brief default destructor
  ~BinaryMatchable() {
    identifiers.clear();
    pointers.clear();
    identifiers_image.clear();
  }

//ds functionality
public:

  //! @brief computes the classic Hamming descriptor distance between this and another matchable
  //! @param[in] matchable_query_ the matchable to compare this against
  //! @returns the matching distance as integer
  inline const uint32_t distance(const BinaryMatchable<descriptor_size_bits_>* matchable_query_) const {
    return (matchable_query_->descriptor^this->descriptor).count();
  }

  //! @brief merges a matchable with THIS matchable (desirable when having to store identical descriptors)
  //! @param[in] matchable_ the matchable to merge with THIS
  inline void merge(const BinaryMatchable<descriptor_size_bits_>* matchable_) {
    identifiers.insert(matchable_->identifiers.begin(), matchable_->identifiers.end());
    pointers.insert(matchable_->pointers.begin(), matchable_->pointers.end());
    identifiers_image.insert(identifiers_image.begin(), matchable_->identifiers_image.begin(), matchable_->identifiers_image.end());
  }

  //! @brief merges a matchable with THIS matchable (desirable when having to store identical descriptors)
  //! @brief this method has been created for quick matchable merging where matchable_ only contains a single entry for identifier and pointer
  //! @param[in] matchable_ the matchable to merge with THIS
  inline void mergeSingle(const BinaryMatchable<descriptor_size_bits_>* matchable_) {
    identifiers.insert(std::make_pair(_identifier_image, _identifier));
    pointers.insert(std::make_pair(_identifier_image, _pointer));
    identifiers_image.push_back(_identifier_image);
  }

#ifdef SRRG_HBST_HAS_OPENCV
  //! @brief descriptor wrapping - only available if OpenCV is present on building system
  //! @param[in] descriptor_cv_ opencv descriptor to convert into HBST format
  static inline Descriptor getDescriptor(const cv::Mat& descriptor_cv_) {

    //ds buffer
    Descriptor binary_descriptor(descriptor_size_bits_);

    //ds set original descriptor string after augmentation
    for (uint64_t byte_index = 0; byte_index < raw_descriptor_size_bytes; ++byte_index) {
      const uint32_t bit_index_start = byte_index*8;

      //ds grab a byte and convert it to a bitset so we can access the single bits
      const std::bitset<8> descriptor_byte(descriptor_cv_.at<uchar>(byte_index));

      //ds set bitstring
      for (uint8_t v = 0; v < 8; ++v) {
        binary_descriptor[bit_index_start+v] = descriptor_byte[v];
      }
    }

    //ds check if we have extra bits (less than 1 byte i.e. <= 7 bits)
    if (descriptor_size_bits_overflow > 0) {

      //ds get last byte (not fully set)
      const std::bitset<8> descriptor_byte(descriptor_cv_.at<uchar>(raw_descriptor_size_bytes));

      //ds only set the remaining bits
      for(uint32_t v = 0; v < descriptor_size_bits_overflow; ++v) {
        binary_descriptor[descriptor_size_bits_in_bytes+v] = descriptor_byte[8-descriptor_size_bits_overflow+v];
      }
    }
    return binary_descriptor;
  }
#endif

//ds attributes
public:

  //! @brief descriptor data string vector
  const Descriptor descriptor;

  //! @brief image reference indices corresponding to this matchable
  std::vector<uint64_t> identifiers_image;

  //! @brief descriptor reference indices corresponding to this matchable - accessible through image identifier
  std::map<uint64_t, uint64_t> identifiers;

  //! @brief a connected object correspondences - when using this field one must ensure the permanence of the referenced object! - accessible through image identifier
  std::map<uint64_t, void*> pointers;

//ds fast access (for a matchable with only single values, internal only)
private:

  //! @brief single value access only: reference image identifier
  const uint64_t _identifier_image;

  //! @brief single value access only: reference descriptor identifier
  const uint64_t _identifier;

  //! @brief single value access only: reference descriptor memory
  void* _pointer;
};

typedef BinaryMatchable<512> BinaryMatchable512;
typedef BinaryMatchable<256> BinaryMatchable256;
typedef BinaryMatchable<128> BinaryMatchable128;
}
