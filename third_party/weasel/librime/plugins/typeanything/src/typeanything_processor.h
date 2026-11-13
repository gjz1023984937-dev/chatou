// TypeAnything processor — captures committed Chinese, calls the LLM on F8,
// and returns a guarded replacement command to the Weasel TSF client.

#ifndef TYPEANYTHING_PROCESSOR_H_
#define TYPEANYTHING_PROCESSOR_H_

#include <rime/processor.h>
#include <rime/component.h>
#include <rime/common.h>
#include <boost/signals2/connection.hpp>
#include <atomic>
#include <memory>
#include <string>

namespace rime {
class Context;
}

namespace typeanything {

class TypeAnythingProcessor : public rime::Processor {
 public:
  explicit TypeAnythingProcessor(const rime::Ticket& ticket);
  ~TypeAnythingProcessor() override;

  rime::ProcessResult ProcessKeyEvent(const rime::KeyEvent& key_event) override;

 private:
  void LoadConfig();
  std::string ResolveTargetLang() const;
  void OnCommit(rime::Context* ctx);
  // Async dispatch — spawns a worker and leaves the original text visible.
  void DispatchTranslate(const std::string& chinese);

  std::string api_key_;
  std::string model_;
  std::string endpoint_;
  std::string endpoint_path_;
  std::string default_target_lang_;
  double temperature_ = 0.3;

  std::string accumulated_;

  // Detached HTTP workers never retain the processor itself. The shared
  // state outlives the processor just long enough for a worker to observe
  // cancellation and exit safely.
  struct AsyncState;
  std::shared_ptr<AsyncState> async_state_;

  boost::signals2::connection commit_conn_;
};

}  // namespace typeanything

#endif  // TYPEANYTHING_PROCESSOR_H_
