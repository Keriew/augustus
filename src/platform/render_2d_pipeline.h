#ifndef PLATFORM_RENDER_2D_PIPELINE_H
#define PLATFORM_RENDER_2D_PIPELINE_H

#include "graphics/renderer.h"

class Render2DPipeline {
public:
    float scale_for_domain(render_domain domain, int ui_scale_percentage) const;
    int scale_logical_size(render_domain domain, int logical_size, int ui_scale_percentage) const;
    float source_scale_x(const render_2d_request &request, const image &img) const;
    float source_scale_y(const render_2d_request &request, const image &img) const;
    render_domain tooltip_domain_for(render_domain domain) const;
    render_domain snapshot_domain_for(render_domain domain) const;
    int should_use_linear_filter(
        const render_2d_request &request,
        const image &img,
        float city_scale,
        int disable_linear_filter) const;
};

#endif // PLATFORM_RENDER_2D_PIPELINE_H
