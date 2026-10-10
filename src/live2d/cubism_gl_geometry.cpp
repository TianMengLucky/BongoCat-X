#include "cubism_plugin_services.hpp"
#include "cubism_model.hpp"
namespace bongo_cat {
void NativeModel::prepare_gl_geometry() {
    size_t vertices = 0, indices = 0;
    for (int i = 0; i < _model->GetDrawableCount(); ++i) {
        vertices += (size_t)_model->GetDrawableVertexCount(i);
        indices += (size_t)_model->GetDrawableVertexIndexCount(i);
    }
    CoreProfileBinding::prepare_begin(vertices, indices);
    for (int i = 0; i < _model->GetDrawableCount(); ++i)
        CoreProfileBinding::prepare_mesh(_model->GetDrawableVertices(i),
            reinterpret_cast<const float *>(_model->GetDrawableVertexUvs(i)),
            (size_t)_model->GetDrawableVertexCount(i), _model->GetDrawableVertexIndices(i),
            (size_t)_model->GetDrawableVertexIndexCount(i));
    CoreProfileBinding::prepare_end();
}
}
