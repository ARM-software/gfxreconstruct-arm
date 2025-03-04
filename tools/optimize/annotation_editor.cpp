/*
** Copyright (c) 2020 LunarG, Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and associated documentation files (the "Software"),
** to deal in the Software without restriction, including without limitation
** the rights to use, copy, modify, merge, publish, distribute, sublicense,
** and/or sell copies of the Software, and to permit persons to whom the
** Software is furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
*/
#include "annotation_editor.h"
#include "decode/file_transformer.h"
#include "util/defines.h"
#include "format/format_util.h"

#include <unordered_set>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)

bool AnnotationEditor::Process()
{
    bool success = decode::FileTransformer::Process();
    if (success)
    {
        for (const auto& annotation : annotations_to_set_)
        {
            // add new annotations at the end
            success = success && WriteAnnotation(annotation.second.first, annotation.first, annotation.second.second);
        }
    }
    return success;
}

void AnnotationEditor::SetAnnotation(format::AnnotationType type, std::string label, std::string data)
{
    annotations_to_set_[label] = { type, data };
}

bool AnnotationEditor::ProcessAnnotation(const format::AnnotationHeader& header,
                                         const std::string&              label,
                                         const std::string&              data)
{
    bool        success                 = true;
    const auto& annotation_modification = annotations_to_set_.find(label);

    if (annotation_modification != annotations_to_set_.end())
    {
        // remove annotation if data is empty
        if (!data.empty())
        {
            // replace existing annotation data
            success = FileTransformer::ProcessAnnotation(header, label, annotation_modification->second.second);
        }
        annotations_to_set_.erase(annotation_modification);
    }
    else
    {
        // keep exsiting annotation
        success = FileTransformer::ProcessAnnotation(header, label, data);
    }
    return success;
}

bool AnnotationEditor::WriteAnnotation(format::AnnotationType annotation_type, std::string label, std::string data)
{
    format::AnnotationHeader header;
    header.block_header.type = format::BlockType::kAnnotation;
    header.block_header.size = format::GetAnnotationBlockBaseSize() + label.size() + data.size();
    header.annotation_type   = annotation_type;
    header.label_length      = label.size();
    header.data_length       = data.size();

    return FileTransformer::ProcessAnnotation(header, label, data);
}

GFXRECON_END_NAMESPACE(gfxrecon)
