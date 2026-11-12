#include "stdafx.h"
#include "WeaselTSF.h"
#include "CandidateList.h"
#include "ResponseParser.h"

namespace {

bool ParseChatouReplaceCommand(const std::wstring& commit,
                               std::wstring* original,
                               std::wstring* replacement) {
  static const std::wstring kPrefix =
      L"\xE000" L"CHATOU_REPLACE_V1" L"\xE001";
  static const wchar_t kSeparator = L'\xE002';
  if (commit.compare(0, kPrefix.size(), kPrefix) != 0)
    return false;
  size_t split = commit.find(kSeparator, kPrefix.size());
  if (split == std::wstring::npos)
    return false;
  *original = commit.substr(kPrefix.size(), split - kPrefix.size());
  *replacement = commit.substr(split + 1);
  return !original->empty() && !replacement->empty();
}

}  // namespace

STDAPI WeaselTSF::DoEditSession(TfEditCookie ec) {
  // get commit string from server
  std::wstring commit;
  weasel::Config config;
  auto context = std::make_shared<weasel::Context>();
  weasel::ResponseParser parser(&commit, context.get(), &_status, &config,
                                &_cand->style());

  bool ok = m_client.GetResponseData(std::ref(parser));

  _UpdateLanguageBar(_status);

  if (ok) {
    if (!commit.empty()) {
      std::wstring chatouOriginal;
      std::wstring chatouReplacement;
      if (ParseChatouReplaceCommand(commit, &chatouOriginal,
                                    &chatouReplacement)) {
        _committed = _ApplyChatouReplacement(
            ec, _pEditSessionContext, chatouOriginal, chatouReplacement, TRUE);
        _UpdateUI(*context, _status);
        return TRUE;
      }
      // For auto-selecting, commit and preedit can both exist.
      // Commit and close the original composition first.
      if (!_IsComposing()) {
        _StartComposition(_pEditSessionContext,
                          _fCUASWorkaroundEnabled && !config.inline_preedit);
      }
      _InsertText(_pEditSessionContext, commit);
      _EndComposition(_pEditSessionContext, false);
      _committed = TRUE;
    } else {
      _committed = FALSE;
    }
    if (_status.composing && !_IsComposing()) {
      _StartComposition(_pEditSessionContext,
                        _fCUASWorkaroundEnabled && !config.inline_preedit);
    } else if (!_status.composing && _IsComposing()) {
      _EndComposition(_pEditSessionContext, true);
    }
    if (_IsComposing() && config.inline_preedit) {
      _ShowInlinePreedit(_pEditSessionContext, context);
    }
    _UpdateCompositionWindow(_pEditSessionContext);
  }

  _UpdateUI(*context, _status);

  return TRUE;
}
