#include "journal/profiles.hpp"

#include <fstream>
#include <list>
#include <string>
#include <type_traits>
#include <vector>

#include "base/map_util.hpp"
#include "glog/logging.h"

namespace principia {

using base::FindOrDie;

namespace journal {
namespace {

template<typename T>
void Insert(not_null<Player::PointerMap*> const pointer_map,
            std::uint64_t const address,
            T* const pointer) {
  void* const inserted_pointer =
      static_cast<void*>(const_cast<std::remove_cv<T>::type*>(pointer));
  auto inserted = pointer_map->emplace(address, inserted_pointer);
  if (!inserted.second) {
    CHECK_EQ(inserted.first->second, inserted_pointer);
  }
}

void Delete(not_null<Player::PointerMap*> const pointer_map,
            std::uint64_t const address) {
  if (reinterpret_cast<void*>(address) != nullptr) {
    auto const it = pointer_map->find(address);
    CHECK(it != pointer_map->end()) << address;
    pointer_map->erase(it);
  }
}

template<typename T,
         typename = typename std::enable_if<std::is_pointer<T>::value>::type>
T DeserializePointer(Player::PointerMap const& pointer_map,
                     std::uint64_t const address) {
  if (reinterpret_cast<T>(address) == nullptr) {
    return nullptr;
  } else {
    return reinterpret_cast<T>(FindOrDie(pointer_map, address));
  }
}

template<typename T>
std::uint64_t SerializePointer(T* t) {
  return reinterpret_cast<std::uint64_t>(t);
}

}  // namespace

#include "journal/profiles.generated.cc"

}  // namespace journal
}  // namespace principia
