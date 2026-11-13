#include "stdafx.h"
#include "WeaselIPC.h"
#include "WeaselTSF.h"
#include <KeyEvent.h>
#include "CandidateList.h"

static weasel::KeyEvent prevKeyEvent;
static BOOL prevfEaten = FALSE;
static int keyCountToSimulate = 0;

namespace {
bool IsChatouUndoKey(WPARAM wParam) {
  return (wParam == 'Z') &&
         ((GetKeyState(VK_CONTROL) & 0x8000) != 0) &&
         ((GetKeyState(VK_SHIFT) & 0x8000) == 0) &&
         ((GetKeyState(VK_MENU) & 0x8000) == 0);
}

bool IsModifierKey(WPARAM wParam) {
  return wParam == VK_CONTROL || wParam == VK_LCONTROL ||
         wParam == VK_RCONTROL || wParam == VK_SHIFT ||
         wParam == VK_LSHIFT || wParam == VK_RSHIFT ||
         wParam == VK_MENU || wParam == VK_LMENU || wParam == VK_RMENU;
}
}  // namespace

void WeaselTSF::_ProcessKeyEvent(WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
  if (static_cast<ULONG_PTR>(GetMessageExtraInfo()) ==
      CHATOU_INJECTED_INPUT_TAG) {
    *pfEaten = FALSE;
    return;
  }

  // when _IsKeyboardDisabled don't eat the key,
  // when keyboard closable and keyboard closed, don't eat the key
  if ((_isToOpenClose && !_IsKeyboardOpen()) || _IsKeyboardDisabled()) {
    *pfEaten = FALSE;
    return;
  }

  // if server connection is Not OK, don't eat it.
  if (!_EnsureServerConnected()) {
    *pfEaten = FALSE;
    return;
  }
  weasel::KeyEvent ke;
  GetKeyboardState(_lpbKeyState);
  if (!ConvertKeyEvent(static_cast<UINT>(wParam), lParam, _lpbKeyState, ke)) {
    /* Unknown key event */
    *pfEaten = FALSE;
  } else {
    // cheet key code when vertical auto reverse happened, swap up and down
    if (_cand->GetIsReposition()) {
      if (ke.keycode == ibus::Up)
        ke.keycode = ibus::Down;
      else if (ke.keycode == ibus::Down)
        ke.keycode = ibus::Up;
    }
    if (!keyCountToSimulate)
      *pfEaten = (BOOL)m_client.ProcessKeyEvent(ke);

    if (ke.keycode == ibus::Caps_Lock) {
      if (prevKeyEvent.keycode == ibus::Caps_Lock && prevfEaten == TRUE &&
          (ke.mask & ibus::RELEASE_MASK) && (!keyCountToSimulate)) {
        if ((GetKeyState(VK_CAPITAL) & 0x01)) {
          if (_committed || (!*pfEaten && _status.composing)) {
            keyCountToSimulate = 2;
            INPUT inputs[2];
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki = {VK_CAPITAL, 0, 0, 0, 0};
            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki = {VK_CAPITAL, 0, KEYEVENTF_KEYUP, 0, 0};
            ::SendInput(sizeof(inputs) / sizeof(INPUT), inputs, sizeof(INPUT));
          }
        }
        *pfEaten = TRUE;
      }
      if (keyCountToSimulate)
        keyCountToSimulate--;
    }

    prevfEaten = *pfEaten;
    prevKeyEvent = ke;
  }
}

STDAPI WeaselTSF::OnSetFocus(BOOL fForeground) {
  if (fForeground)
    m_client.FocusIn();
  else {
    _InvalidateChatouUndo();
    m_client.FocusOut();
    _AbortComposition();
  }

  return S_OK;
}

/* Some apps sends strange OnTestKeyDown/OnKeyDown combinations:
 *  Some sends OnKeyDown() only. (QQ2012)
 *  Some sends multiple OnTestKeyDown() for a single key event. (MS WORD 2010
 * x64)
 *
 * We assume every key event will eventually cause a OnKeyDown() call.
 * We use _fTestKeyDownPending to omit multiple OnTestKeyDown() calls,
 *  and for OnKeyDown() to check if the key has already been sent to the server.
 */

STDAPI WeaselTSF::OnTestKeyDown(ITfContext* pContext,
                                WPARAM wParam,
                                LPARAM lParam,
                                BOOL* pfEaten) {
  _fTestKeyUpPending = FALSE;
  if (_fTestKeyDownPending) {
    *pfEaten = TRUE;
    return S_OK;
  }
  if (IsChatouUndoKey(wParam) && _UndoChatouReplacement(pContext)) {
    *pfEaten = TRUE;
    _fTestKeyDownPending = TRUE;
    return S_OK;
  }
  if (!IsModifierKey(wParam) &&
      static_cast<ULONG_PTR>(GetMessageExtraInfo()) !=
          CHATOU_INJECTED_INPUT_TAG)
    _InvalidateChatouUndo();
  _ProcessKeyEvent(wParam, lParam, pfEaten);
  _UpdateComposition(pContext);
  if (*pfEaten)
    _fTestKeyDownPending = TRUE;
  return S_OK;
}

STDAPI WeaselTSF::OnKeyDown(ITfContext* pContext,
                            WPARAM wParam,
                            LPARAM lParam,
                            BOOL* pfEaten) {
  _fTestKeyUpPending = FALSE;
  if (_fTestKeyDownPending) {
    _fTestKeyDownPending = FALSE;
    *pfEaten = TRUE;
  } else {
    if (IsChatouUndoKey(wParam) && _UndoChatouReplacement(pContext)) {
      *pfEaten = TRUE;
      return S_OK;
    }
    if (!IsModifierKey(wParam) &&
        static_cast<ULONG_PTR>(GetMessageExtraInfo()) !=
            CHATOU_INJECTED_INPUT_TAG)
      _InvalidateChatouUndo();
    _ProcessKeyEvent(wParam, lParam, pfEaten);
    _UpdateComposition(pContext);
  }
  return S_OK;
}

STDAPI WeaselTSF::OnTestKeyUp(ITfContext* pContext,
                              WPARAM wParam,
                              LPARAM lParam,
                              BOOL* pfEaten) {
  _fTestKeyDownPending = FALSE;
  if (_fTestKeyUpPending) {
    *pfEaten = TRUE;
    return S_OK;
  }
  _ProcessKeyEvent(wParam, lParam, pfEaten);
  _UpdateComposition(pContext);
  if (*pfEaten)
    _fTestKeyUpPending = TRUE;
  return S_OK;
}

STDAPI WeaselTSF::OnKeyUp(ITfContext* pContext,
                          WPARAM wParam,
                          LPARAM lParam,
                          BOOL* pfEaten) {
  _fTestKeyDownPending = FALSE;
  if (_fTestKeyUpPending) {
    _fTestKeyUpPending = FALSE;
    *pfEaten = TRUE;
  } else {
    _ProcessKeyEvent(wParam, lParam, pfEaten);
    if (!_async_edit)
      _UpdateComposition(pContext);
  }
  return S_OK;
}

STDAPI WeaselTSF::OnPreservedKey(ITfContext* pContext,
                                 REFGUID rguid,
                                 BOOL* pfEaten) {
  *pfEaten = FALSE;
  if (!IsEqualGUID(rguid, GUID_IME_MODE_PRESERVED_KEY))
    return S_OK;

  // Function keys are commonly handled as application accelerators before
  // the normal TSF key-event sink sees them. Registering F8 as a preserved
  // key routes it here while Weasel is active. Feed the same IBus keysym to
  // librime that ConvertKeyEvent(VK_F8, ...) would have produced.
  if (!_EnsureServerConnected())
    return S_OK;

  weasel::KeyEvent key(ibus::F8, ibus::NULL_MASK);
  *pfEaten = m_client.ProcessKeyEvent(key) ? TRUE : FALSE;
  _UpdateComposition(pContext);
  return S_OK;
}

BOOL WeaselTSF::_InitKeyEventSink() {
  com_ptr<ITfKeystrokeMgr> pKeystrokeMgr;
  HRESULT hr;

  if (_pThreadMgr->QueryInterface(&pKeystrokeMgr) != S_OK)
    return FALSE;

  hr = pKeystrokeMgr->AdviseKeyEventSink(_tfClientId, (ITfKeyEventSink*)this,
                                         TRUE);

  return (hr == S_OK);
}

void WeaselTSF::_UninitKeyEventSink() {
  com_ptr<ITfKeystrokeMgr> pKeystrokeMgr;

  if (_pThreadMgr->QueryInterface(&pKeystrokeMgr) != S_OK)
    return;

  pKeystrokeMgr->UnadviseKeyEventSink(_tfClientId);
}

BOOL WeaselTSF::_InitPreservedKey() {
  com_ptr<ITfKeystrokeMgr> pKeystrokeMgr;
  if (_pThreadMgr->QueryInterface(&pKeystrokeMgr) != S_OK)
    return FALSE;

  TF_PRESERVEDKEY preservedKey = {};
  preservedKey.uVKey = VK_F8;
  preservedKey.uModifiers = 0;
  static const WCHAR description[] = L"Chatou AI translation";

  // Clear a stale registration left by an earlier activation in the same
  // process, then install the current one.
  pKeystrokeMgr->UnpreserveKey(GUID_IME_MODE_PRESERVED_KEY,
                               &preservedKey);
  HRESULT hr = pKeystrokeMgr->PreserveKey(
      _tfClientId,
      GUID_IME_MODE_PRESERVED_KEY,
      &preservedKey,
      description,
      ARRAYSIZE(description) - 1);
  return SUCCEEDED(hr);
}

void WeaselTSF::_UninitPreservedKey() {
  if (!_pThreadMgr || _tfClientId == TF_CLIENTID_NULL)
    return;

  com_ptr<ITfKeystrokeMgr> pKeystrokeMgr;
  if (_pThreadMgr->QueryInterface(&pKeystrokeMgr) != S_OK)
    return;

  TF_PRESERVEDKEY preservedKey = {};
  preservedKey.uVKey = VK_F8;
  preservedKey.uModifiers = 0;
  pKeystrokeMgr->UnpreserveKey(GUID_IME_MODE_PRESERVED_KEY,
                               &preservedKey);
}
