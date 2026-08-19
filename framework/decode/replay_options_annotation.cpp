#include "decode/replay_options_annotation.h"
#include "decode/file_processor.h"

#include <sstream>
#include <iterator>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

void ReplayOptionsAnnotationHandler::ProcessAnnotation(uint64_t                         block_index,
                                                       gfxrecon::format::AnnotationType type,
                                                       const std::string&               label,
                                                       const std::string&               data)
{
    if (label == format::kAnnotationLabelReplayOptions)
    {
        replay_options_ = data;
    }
}

std::string ReplayOptionsAnnotationHandler::GetReplayOptions()
{
    return replay_options_;
}

// Returns a vector of replay arguments saved in trace annotation
std::vector<std::string> GetTraceReplayOptions(const std::string& filename)
{
    gfxrecon::decode::FileProcessor trace_options_procesor;
    trace_options_procesor.Initialize(filename);
    ReplayOptionsAnnotationHandler annotation_handler;
    trace_options_procesor.SetAnnotationProcessor(&annotation_handler);

    bool success = trace_options_procesor.ProcessReplayOptionsAnnotation();
    if (success)
    {
        std::stringstream                  ss(annotation_handler.GetReplayOptions());
        std::istream_iterator<std::string> begin(ss);
        std::istream_iterator<std::string> end;
        std::vector<std::string>           command_line_args(begin, end);
        return command_line_args;
    }
    else
    {
        return {};
    }
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)