#include <variant>
namespace step_result
{
struct Done;
struct Ready;
struct Wait;
struct CompositeWait;
} // namespace step_result
using StepResult =
    std::variant<step_result::Done, step_result::Ready, step_result::Wait,
                 step_result::CompositeWait>;