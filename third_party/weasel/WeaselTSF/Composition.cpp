#include "stdafx.h"
#include "WeaselTSF.h"
#include "EditSession.h"
#include "ResponseParser.h"
#include "CandidateList.h"
#include <KeyEvent.h>
#include <cwctype>
#include <vector>

namespace {

bool IsNativeEditControl(HWND hwnd) {
  wchar_t className[128] = {};
  if (!hwnd || !GetClassNameW(hwnd, className, _countof(className)))
    return false;
  std::wstring name(className);
  for (wchar_t& ch : name)
    ch = static_cast<wchar_t>(std::towlower(ch));
  return name == L"edit" || name.find(L"richedit") != std::wstring::npos;
}

bool ReplaceInNativeEdit(HWND hwnd,
                         const std::wstring& expected,
                         const std::wstring& replacement) {
  if (!IsNativeEditControl(hwnd))
    return false;

  DWORD selectionStart = 0;
  DWORD selectionEnd = 0;
  SendMessageW(hwnd, EM_GETSEL, reinterpret_cast<WPARAM>(&selectionStart),
               reinterpret_cast<LPARAM>(&selectionEnd));
  if (selectionStart != selectionEnd || selectionStart < expected.size())
    return false;

  int textLength = GetWindowTextLengthW(hwnd);
  if (textLength < 0 || textLength > 1024 * 1024 ||
      selectionStart > static_cast<DWORD>(textLength)) {
    return false;
  }
  std::vector<wchar_t> text(static_cast<size_t>(textLength) + 1, L'\0');
  int copied = GetWindowTextW(hwnd, text.data(), textLength + 1);
  if (copied < 0 || selectionStart > static_cast<DWORD>(copied))
    return false;

  size_t sourceStart = selectionStart - expected.size();
  if (std::wstring(text.data() + sourceStart, expected.size()) != expected)
    return false;

  SendMessageW(hwnd, EM_SETSEL, sourceStart, selectionEnd);
  SendMessageW(hwnd, EM_REPLACESEL, TRUE,
               reinterpret_cast<LPARAM>(replacement.c_str()));
  return true;
}

size_t Utf16CharacterCount(const std::wstring& text) {
  size_t count = 0;
  for (size_t i = 0; i < text.size(); ++i) {
    wchar_t ch = text[i];
    if (ch >= 0xD800 && ch <= 0xDBFF && i + 1 < text.size() &&
        text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF) {
      ++i;
    }
    ++count;
  }
  return count;
}

void AppendVirtualKey(std::vector<INPUT>* inputs, WORD key, DWORD flags = 0) {
  INPUT input = {};
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = key;
  input.ki.dwFlags = flags;
  input.ki.dwExtraInfo = CHATOU_INJECTED_INPUT_TAG;
  inputs->push_back(input);
}

void AppendUnicode(std::vector<INPUT>* inputs, wchar_t ch, DWORD flags = 0) {
  INPUT input = {};
  input.type = INPUT_KEYBOARD;
  input.ki.wScan = ch;
  input.ki.dwFlags = KEYEVENTF_UNICODE | flags;
  input.ki.dwExtraInfo = CHATOU_INJECTED_INPUT_TAG;
  inputs->push_back(input);
}

bool ReplaceWithKeyboardInput(const std::wstring& expected,
                              const std::wstring& replacement) {
  if (!GetFocus() || expected.empty() || expected.size() > 4096 ||
      replacement.size() > 4096) {
    return false;
  }

  size_t sourceCharacters = Utf16CharacterCount(expected);
  std::vector<INPUT> inputs;
  inputs.reserve(2 + sourceCharacters * 2 + replacement.size() * 2);

  AppendVirtualKey(&inputs, VK_SHIFT);
  for (size_t i = 0; i < sourceCharacters; ++i) {
    AppendVirtualKey(&inputs, VK_LEFT);
    AppendVirtualKey(&inputs, VK_LEFT, KEYEVENTF_KEYUP);
  }
  AppendVirtualKey(&inputs, VK_SHIFT, KEYEVENTF_KEYUP);

  for (wchar_t ch : replacement) {
    AppendUnicode(&inputs, ch);
    AppendUnicode(&inputs, ch, KEYEVENTF_KEYUP);
  }

  return !inputs.empty() &&
         SendInput(static_cast<UINT>(inputs.size()), inputs.data(),
                   sizeof(INPUT)) == inputs.size();
}

}  // namespace

/* Start Composition */
class CStartCompositionEditSession : public CEditSession {
 public:
  CStartCompositionEditSession(com_ptr<WeaselTSF> pTextService,
                               com_ptr<ITfContext> pContext,
                               BOOL fCUASWorkaroundEnabled,
                               BOOL inlinePreeditEnabled)
      : CEditSession(pTextService, pContext),
        _inlinePreeditEnabled(inlinePreeditEnabled) {
    _fCUASWorkaroundEnabled = fCUASWorkaroundEnabled;
  }

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  BOOL _fCUASWorkaroundEnabled;
  BOOL _inlinePreeditEnabled;
};

STDAPI CStartCompositionEditSession::DoEditSession(TfEditCookie ec) {
  HRESULT hr = E_FAIL;
  com_ptr<ITfInsertAtSelection> pInsertAtSelection;
  com_ptr<ITfRange> pRangeComposition;
  if (_pContext->QueryInterface(IID_ITfInsertAtSelection,
                                (LPVOID*)&pInsertAtSelection) != S_OK)
    return hr;
  if (pInsertAtSelection->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, NULL, 0,
                                                &pRangeComposition) != S_OK)
    return hr;

  com_ptr<ITfContextComposition> pContextComposition;
  com_ptr<ITfComposition> pComposition;
  if (_pContext->QueryInterface(IID_ITfContextComposition,
                                (LPVOID*)&pContextComposition) != S_OK)
    return hr;
  if ((pContextComposition->StartComposition(
           ec, pRangeComposition, _pTextService, &pComposition) == S_OK) &&
      (pComposition != NULL)) {
    _pTextService->_SetComposition(pComposition);

    /* WORKAROUND:
     *   CUAS does not provide a correct GetTextExt() position unless the
     * composition is filled with characters. So we insert a zero width space
     * here. The workaround is only needed when inline preedit is not enabled.
     *   See https://github.com/rime/weasel/pull/883#issuecomment-1567625762
     */
    if (!_inlinePreeditEnabled) {
      pRangeComposition->SetText(ec, TF_ST_CORRECTION, L" ", 1);
    }

    /* set selection */
    TF_SELECTION tfSelection;
    if (_inlinePreeditEnabled)
      pRangeComposition->Collapse(ec, TF_ANCHOR_END);
    else
      pRangeComposition->Collapse(ec, TF_ANCHOR_START);
    tfSelection.range = pRangeComposition;
    tfSelection.style.ase = TF_AE_NONE;
    tfSelection.style.fInterimChar = FALSE;
    _pContext->SetSelection(ec, 1, &tfSelection);
  }

  return hr;
}

void WeaselTSF::_StartComposition(com_ptr<ITfContext> pContext,
                                  BOOL fCUASWorkaroundEnabled) {
  com_ptr<CStartCompositionEditSession> pStartCompositionEditSession;
  pStartCompositionEditSession.Attach(new CStartCompositionEditSession(
      this, pContext, fCUASWorkaroundEnabled, _cand->style().inline_preedit));
  _cand->StartUI();
  if (pStartCompositionEditSession != nullptr) {
    HRESULT hr;
    pContext->RequestEditSession(_tfClientId, pStartCompositionEditSession,
                                 TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
  }
}

/* End Composition */
class CEndCompositionEditSession : public CEditSession {
 public:
  CEndCompositionEditSession(com_ptr<WeaselTSF> pTextService,
                             com_ptr<ITfContext> pContext,
                             com_ptr<ITfComposition> pComposition,
                             BOOL clear = TRUE)
      : CEditSession(pTextService, pContext), _clear(clear) {
    _pComposition = pComposition;
  }

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  com_ptr<ITfComposition> _pComposition;
  BOOL _clear;
};

STDAPI CEndCompositionEditSession::DoEditSession(TfEditCookie ec) {
  /* Clear the dummy text we set before, if any. */
  if (_pComposition == nullptr)
    return S_OK;
  // Avoid null pointer dereference
  if (!_pTextService || !_pContext)
    return S_OK;

  _pTextService->_ClearCompositionDisplayAttributes(ec, _pContext);

  com_ptr<ITfRange> pCompositionRange;
  if (_clear && _pComposition->GetRange(&pCompositionRange) == S_OK)
    pCompositionRange->SetText(ec, 0, L"", 0);

  _pComposition->EndComposition(ec);
  if (_pTextService)  // if _pTextService released, skip _FinalizeComposition
    _pTextService->_FinalizeComposition();
  return S_OK;
}

void WeaselTSF::_EndComposition(com_ptr<ITfContext> pContext, BOOL clear) {
  CEndCompositionEditSession* pEditSession;
  HRESULT hr;

  _cand->EndUI();
  if ((pEditSession = new CEndCompositionEditSession(
           this, pContext, _pComposition, clear)) != NULL) {
    pContext->RequestEditSession(_tfClientId, pEditSession,
                                 TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
    pEditSession->Release();
  }
}

/* Get Text Extent */
class CGetTextExtentEditSession : public CEditSession {
 public:
  CGetTextExtentEditSession(com_ptr<WeaselTSF> pTextService,
                            com_ptr<ITfContext> pContext,
                            com_ptr<ITfContextView> pContextView,
                            com_ptr<ITfComposition> pComposition,
                            bool enhancedPosition)
      : CEditSession(pTextService, pContext) {
    _pContextView = pContextView;
    _pComposition = pComposition;
    _enhancedPosition = enhancedPosition;
  }

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  com_ptr<ITfContextView> _pContextView;
  com_ptr<ITfComposition> _pComposition;
  bool _enhancedPosition;
};

STDAPI CGetTextExtentEditSession::DoEditSession(TfEditCookie ec) {
  com_ptr<ITfInsertAtSelection> pInsertAtSelection;
  com_ptr<ITfRange> pRangeComposition;
  ITfRange* pRange;
  RECT rc;
  BOOL fClipped;
  TF_SELECTION selection;
  ULONG nSelection;

  if (FAILED(_pContext->QueryInterface(IID_ITfInsertAtSelection,
                                       (LPVOID*)&pInsertAtSelection)))
    return E_FAIL;
  if (FAILED(_pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &selection,
                                     &nSelection)))
    return E_FAIL;

  if (_pComposition != nullptr && _pComposition->GetRange(&pRange) == S_OK) {
    pRange->Collapse(ec, TF_ANCHOR_START);
  } else {
    // composition end
    // note: selection.range is always an empty range
    pRange = selection.range;
  }

  if ((_pContextView->GetTextExt(ec, pRange, &rc, &fClipped)) == S_OK &&
      (rc.left != 0 || rc.top != 0)) {
    // get the foreground window pos and check if rc from GetTextExt is out of
    // window
    if (_enhancedPosition) {
      HWND hwnd;
      RECT rcForegroundWindow;
      hwnd = GetForegroundWindow();
      ::GetWindowRect(hwnd, &rcForegroundWindow);

      if (rc.left < rcForegroundWindow.left ||
          rc.left > rcForegroundWindow.right ||
          rc.top < rcForegroundWindow.top ||
          rc.top > rcForegroundWindow.bottom) {
        POINT pt;
        bool hasCaret = ::GetCaretPos(&pt);
        int offsetx = rcForegroundWindow.left - rc.left + (hasCaret ? pt.x : 0);
        int offsety = rcForegroundWindow.top - rc.top + (hasCaret ? pt.y : 0);
        rc.left += offsetx;
        rc.right += offsetx;
        rc.top += offsety;
        rc.bottom += offsety;
      }
    }
    _pTextService->_SetCompositionPosition(rc);
  }
  return S_OK;
}

/* Composition Window Handling */
BOOL WeaselTSF::_UpdateCompositionWindow(com_ptr<ITfContext> pContext) {
  com_ptr<ITfContextView> pContextView;
  if (pContext->GetActiveView(&pContextView) != S_OK)
    return FALSE;
  com_ptr<CGetTextExtentEditSession> pEditSession;
  pEditSession.Attach(
      new CGetTextExtentEditSession(this, pContext, pContextView, _pComposition,
                                    _cand->style().enhanced_position));
  if (pEditSession == NULL) {
    return FALSE;
  }
  HRESULT hr;
  pContext->RequestEditSession(_tfClientId, pEditSession,
                               TF_ES_ASYNCDONTCARE | TF_ES_READ, &hr);
  return SUCCEEDED(hr);
}

void WeaselTSF::_SetCompositionPosition(const RECT& rc) {
  /* Test if rect is valid.
   * If it is invalid during CUAS test, we need to apply CUAS workaround */
  if (!_fCUASWorkaroundTested) {
    _fCUASWorkaroundTested = TRUE;
    if (rc.top == rc.bottom) {
      _fCUASWorkaroundEnabled = TRUE;
      return;
    }
  }
  RECT _rc;
  _rc.left = _rc.right = rc.left;
  _rc.top = _rc.bottom = rc.bottom;
  m_client.UpdateInputPosition(rc);
  _cand->UpdateInputPosition(rc);
}

/* Inline Preedit */
class CInlinePreeditEditSession : public CEditSession {
 public:
  CInlinePreeditEditSession(com_ptr<WeaselTSF> pTextService,
                            com_ptr<ITfContext> pContext,
                            com_ptr<ITfComposition> pComposition,
                            const std::shared_ptr<weasel::Context> context)
      : CEditSession(pTextService, pContext),
        _pComposition(pComposition),
        _context(context) {}

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  com_ptr<ITfComposition> _pComposition;
  const std::shared_ptr<weasel::Context> _context;
};

STDAPI CInlinePreeditEditSession::DoEditSession(TfEditCookie ec) {
  std::wstring preedit = _context->preedit.str;

  com_ptr<ITfRange> pRangeComposition;
  if (_pComposition == nullptr)
    return E_FAIL;
  if ((_pComposition->GetRange(&pRangeComposition)) != S_OK)
    return E_FAIL;

  if ((pRangeComposition->SetText(ec, 0, preedit.c_str(),
                                  static_cast<LONG>(preedit.length()))) != S_OK)
    return E_FAIL;

  /* TODO: Check the availability and correctness of these values */
  int sel_cursor = -1;
  for (size_t i = 0; i < _context->preedit.attributes.size(); i++) {
    if (_context->preedit.attributes.at(i).type == weasel::HIGHLIGHTED) {
      sel_cursor = _context->preedit.attributes.at(i).range.cursor;
      break;
    }
  }

  _pTextService->_SetCompositionDisplayAttributes(ec, _pContext,
                                                  pRangeComposition);

  /* Set caret */
  LONG cch;
  TF_SELECTION tfSelection;
  if (sel_cursor < 0) {
    pRangeComposition->Collapse(ec, TF_ANCHOR_END);
  } else {
    pRangeComposition->Collapse(ec, TF_ANCHOR_START);
    pRangeComposition->ShiftStart(ec, sel_cursor, &cch, NULL);
  }
  tfSelection.range = pRangeComposition;
  tfSelection.style.ase = TF_AE_NONE;
  tfSelection.style.fInterimChar = FALSE;
  _pContext->SetSelection(ec, 1, &tfSelection);

  return S_OK;
}

BOOL WeaselTSF::_ShowInlinePreedit(
    com_ptr<ITfContext> pContext,
    const std::shared_ptr<weasel::Context> context) {
  com_ptr<CInlinePreeditEditSession> pEditSession;
  pEditSession.Attach(
      new CInlinePreeditEditSession(this, pContext, _pComposition, context));
  if (pEditSession != NULL) {
    HRESULT hr;
    pContext->RequestEditSession(_tfClientId, pEditSession,
                                 TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
  }
  return TRUE;
}

/* Update Composition */
class CInsertTextEditSession : public CEditSession {
 public:
  CInsertTextEditSession(com_ptr<WeaselTSF> pTextService,
                         com_ptr<ITfContext> pContext,
                         com_ptr<ITfComposition> pComposition,
                         const std::wstring& text)
      : CEditSession(pTextService, pContext),
        _text(text),
        _pComposition(pComposition) {}

  /* ITfEditSession */
  STDMETHODIMP DoEditSession(TfEditCookie ec);

 private:
  std::wstring _text;
  com_ptr<ITfComposition> _pComposition;
};

STDMETHODIMP CInsertTextEditSession::DoEditSession(TfEditCookie ec) {
  com_ptr<ITfRange> pRange;
  TF_SELECTION tfSelection;
  HRESULT hRet = S_OK;

  if (_pComposition == nullptr)
    return E_FAIL;
  if (FAILED(_pComposition->GetRange(&pRange)))
    return E_FAIL;

  if (FAILED(pRange->SetText(ec, 0, _text.c_str(),
                             static_cast<LONG>(_text.length()))))
    return E_FAIL;

  /* update the selection to an insertion point just past the inserted text. */
  pRange->Collapse(ec, TF_ANCHOR_END);

  tfSelection.range = pRange;
  tfSelection.style.ase = TF_AE_NONE;
  tfSelection.style.fInterimChar = FALSE;

  _pContext->SetSelection(ec, 1, &tfSelection);

  return hRet;
}

BOOL WeaselTSF::_InsertText(com_ptr<ITfContext> pContext,
                            const std::wstring& text) {
  CInsertTextEditSession* pEditSession;
  HRESULT hr;

  if ((pEditSession = new CInsertTextEditSession(this, pContext, _pComposition,
                                                 text)) != NULL) {
    pContext->RequestEditSession(_tfClientId, pEditSession,
                                 TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
    pEditSession->Release();
  }

  return TRUE;
}

BOOL WeaselTSF::_ApplyChatouReplacement(TfEditCookie ec,
                                        com_ptr<ITfContext> pContext,
                                        const std::wstring& expected,
                                        const std::wstring& replacement,
                                        BOOL recordUndo) {
  if (!pContext || expected.empty())
    return FALSE;

  TF_SELECTION selection = {};
  ULONG fetchedSelections = 0;
  if (FAILED(pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &selection,
                                    &fetchedSelections)) ||
      fetchedSelections != 1 || !selection.range) {
    return FALSE;
  }

  com_ptr<ITfRange> range;
  range.Attach(selection.range);
  BOOL isEmpty = FALSE;
  if (FAILED(range->IsEmpty(ec, &isEmpty)) || !isEmpty)
    return FALSE;

  LONG shifted = 0;
  if (FAILED(range->ShiftStart(ec, -static_cast<LONG>(expected.size()),
                               &shifted, nullptr)) ||
      shifted != -static_cast<LONG>(expected.size())) {
    return FALSE;
  }

  std::vector<wchar_t> current(expected.size() + 1, L'\0');
  ULONG fetchedText = 0;
  if (FAILED(range->GetText(ec, 0, current.data(),
                            static_cast<ULONG>(expected.size()),
                            &fetchedText)) ||
      fetchedText != expected.size() ||
      std::wstring(current.data(), fetchedText) != expected) {
    return FALSE;
  }

  if (FAILED(range->SetText(ec, TF_ST_CORRECTION, replacement.c_str(),
                            static_cast<LONG>(replacement.size())))) {
    return FALSE;
  }

  range->Collapse(ec, TF_ANCHOR_END);
  TF_SELECTION newSelection = {};
  newSelection.range = range;
  newSelection.style.ase = TF_AE_NONE;
  newSelection.style.fInterimChar = FALSE;
  // Some hosts update the caret themselves after SetText but reject an
  // explicit SetSelection. The text mutation already succeeded, so do not
  // run a second fallback replacement in that case.
  pContext->SetSelection(ec, 1, &newSelection);

  if (recordUndo) {
    _RecordChatouUndo(pContext, expected, replacement);
  } else {
    _InvalidateChatouUndo();
  }
  return TRUE;
}

BOOL WeaselTSF::_ApplyChatouFallback(const std::wstring& expected,
                                     const std::wstring& replacement) {
  HWND focused = GetFocus();
  if (ReplaceInNativeEdit(focused, expected, replacement))
    return TRUE;
  return ReplaceWithKeyboardInput(expected, replacement) ? TRUE : FALSE;
}

void WeaselTSF::_RecordChatouUndo(com_ptr<ITfContext> pContext,
                                  const std::wstring& original,
                                  const std::wstring& result) {
  _chatouUndoContext = pContext;
  _chatouUndoOriginal = original;
  _chatouUndoResult = result;
  _chatouUndoValid = TRUE;
}

class CChatouUndoEditSession : public CEditSession {
 public:
  CChatouUndoEditSession(com_ptr<WeaselTSF> service,
                         com_ptr<ITfContext> context,
                         const std::wstring& result,
                         const std::wstring& original)
      : CEditSession(service, context),
        _result(result),
        _original(original) {}

  STDMETHODIMP DoEditSession(TfEditCookie ec) override {
    return _pTextService->_ApplyChatouReplacement(
               ec, _pContext, _result, _original, FALSE)
               ? S_OK
               : E_FAIL;
  }

 private:
  std::wstring _result;
  std::wstring _original;
};

BOOL WeaselTSF::_UndoChatouReplacement(com_ptr<ITfContext> pContext) {
  if (!_chatouUndoValid || !pContext || pContext != _chatouUndoContext)
    return FALSE;

  std::wstring result = _chatouUndoResult;
  std::wstring original = _chatouUndoOriginal;

  com_ptr<CChatouUndoEditSession> editSession;
  editSession.Attach(
      new CChatouUndoEditSession(this, pContext, result, original));
  if (!editSession)
    return FALSE;

  HRESULT sessionResult = E_FAIL;
  HRESULT requestResult = pContext->RequestEditSession(
      _tfClientId, editSession, TF_ES_SYNC | TF_ES_READWRITE,
      &sessionResult);
  // Only consume Ctrl+Z when the checked restoration completed. If the host
  // refuses a synchronous edit session or the caret/text no longer matches,
  // return FALSE so its native undo stack receives the shortcut unchanged.
  bool restored = SUCCEEDED(requestResult) && SUCCEEDED(sessionResult);
  if (!restored) {
    restored = !!_ApplyChatouFallback(result, original);
    if (restored)
      _InvalidateChatouUndo();
  }
  if (restored && _EnsureServerConnected()) {
    // Tell the plugin that the just-restored source may be translated once
    // more. Drain the response immediately so it cannot be mistaken for the
    // next real key event. Notification failure does not undo the document
    // restoration and therefore must not leak Ctrl+Z to the host.
    weasel::KeyEvent key(ibus::F23, ibus::NULL_MASK);
    m_client.ProcessKeyEvent(key);
    weasel::ResponseParser parser(NULL, NULL, &_status, NULL, &_cand->style());
    m_client.GetResponseData(std::ref(parser));
  }
  return restored;
}

void WeaselTSF::_InvalidateChatouUndo() {
  _chatouUndoValid = FALSE;
  _chatouUndoContext.Release();
  _chatouUndoOriginal.clear();
  _chatouUndoResult.clear();
}

void WeaselTSF::_UpdateComposition(com_ptr<ITfContext> pContext) {
  HRESULT hr;

  _pEditSessionContext = pContext;

  _pEditSessionContext->RequestEditSession(
      _tfClientId, this, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
  _async_edit = !!(hr == TF_S_ASYNC);
  _UpdateCompositionWindow(pContext);
}

/* Composition State */
STDAPI WeaselTSF::OnCompositionTerminated(TfEditCookie ecWrite,
                                          ITfComposition* pComposition) {
  // NOTE:
  // This will be called when an edit session ended up with an empty composition
  // string, Even if it is closed normally. Silly M$.

  _AbortComposition();
  return S_OK;
}

void WeaselTSF::_AbortComposition(bool clear) {
  m_client.ClearComposition();
  if (_IsComposing()) {
    _EndComposition(_pEditSessionContext, clear);
  }
  _committed = TRUE;
  _cand->Destroy();
}

void WeaselTSF::_FinalizeComposition() {
  _pComposition = nullptr;
}

void WeaselTSF::_SetComposition(com_ptr<ITfComposition> pComposition) {
  _pComposition = pComposition;
}

BOOL WeaselTSF::_IsComposing() {
  return _pComposition != NULL;
}
