#include <variant>
namespace step_result
{
    struct Done;
    struct Ready; 
    struct Wait;
    struct PartialWait; 
    struct FullWait;
}
using StepResult = std::variant<
    step_result::Done, 
    step_result::Ready, 
    step_result::Wait, 
    step_result::PartialWait, 
    step_result::FullWait>;