#include "stdafx.h"
#include "ChatouReplacer.h"
#include <WeaselUtility.h>

using namespace weasel;

Deserializer::Ptr ChatouReplacer::Create(ResponseParser* pTarget) {
  return Deserializer::Ptr(new ChatouReplacer(pTarget));
}

ChatouReplacer::ChatouReplacer(ResponseParser* pTarget)
    : Deserializer(pTarget) {}

ChatouReplacer::~ChatouReplacer() {}

void ChatouReplacer::Store(Deserializer::KeyType const& key,
                           std::wstring const& value) {
  if (key.size() != 2)
    return;
  if (key[1] == L"original" && m_pTarget->p_chatou_original) {
    *m_pTarget->p_chatou_original = unescape_string(value);
  } else if (key[1] == L"replacement" &&
             m_pTarget->p_chatou_replacement) {
    *m_pTarget->p_chatou_replacement = unescape_string(value);
  }
}
