#pragma once
#include "Deserializer.h"

class ChatouReplacer : public weasel::Deserializer {
 public:
  ChatouReplacer(weasel::ResponseParser* pTarget);
  virtual ~ChatouReplacer();
  virtual void Store(weasel::Deserializer::KeyType const& key,
                     std::wstring const& value);
  static weasel::Deserializer::Ptr Create(weasel::ResponseParser* pTarget);
};
