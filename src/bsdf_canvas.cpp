#include <tekari/bsdf_canvas.h>

#include <enoki/transform.h>
#include <nanogui/layout.h>
#include <nanogui/screen.h>
#include <string>
#include <tekari/dataset.h>
#include <tekari/arrow.h>

#define MAX_ZOOM 10.0f
#define MIN_ZOOM -MAX_ZOOM

TEKARI_NAMESPACE_BEGIN

const Vector3f BSDFCanvas::VIEW_ORIGIN(0, 0, 4);
const Vector3f BSDFCanvas::VIEW_UP(0, 0, 1);
const Vector3f BSDFCanvas::VIEW_RIGHT(1, 0, 0);
const Matrix4f BSDFCanvas::VIEW(enoki::translate<Matrix4f>(Vector3f(0, 0, -4)));
const Matrix4f BSDFCanvas::MODEL(
     1, 0, 0, 0,
     0, 0, 1, 0,
     0, -1, 0, 0,
     0, 0, 0, 1
);

const float BSDFCanvas::NEAR = 0.01f;
const float BSDFCanvas::FAR = 100.0f;

const int BSDFCanvas::BUTTON_MAPPINGS[2][BSDFCanvas::MOUSE_MODE_COUNT] =
{
    { GLFW_MOUSE_BUTTON_1, GLFW_MOUSE_BUTTON_2, GLFW_MOUSE_BUTTON_3 },
    { GLFW_MOUSE_BUTTON_2, GLFW_MOUSE_BUTTON_3, GLFW_MOUSE_BUTTON_5 }
};

BSDFCanvas::BSDFCanvas(Widget* parent)
:   GLCanvas(parent)
,   m_translation(0, 0, 0)
,   m_zoom(0)
,   m_point_size_scale(2.5f)
,   m_ortho_mode(false)
,   m_mouse_mode(ROTATE)
,   m_selection_region(make_pair(Vector2i(0,0), Vector2i(0,0)))
,   m_draw_flags(DISPLAY_AXIS | USE_SHADOWS)
{
    m_arcball.set_state(enoki::rotate<Quaternion4f>(Vector3f(1, 0, 0), static_cast<float>(M_PI / 4.0)));
}

bool BSDFCanvas::mouse_motion_event(const Vector2i& p,
                              const Vector2i& rel,
                              int button, int modifiers) {
    if (GLCanvas::mouse_motion_event(p, rel, button, modifiers))
        return true;
    if (!focused())
        return false;

    if (modifiers & SYSTEM_COMMAND_MOD && button == GLFW_MOUSE_BUTTON_2)
    {
        m_update_incident_angle_callback(get_incident_angle(p));
        return true;
    }
    
    if (button == rotation_mouse_button(true))
    {
        m_arcball.motion(p);
        return true;
    }
    else if (button == selection_mouse_button(true))
    {
        m_selection_region.second = p;
        return true;
    }
    else if (button == translation_mouse_button(true))
    {
        float move_speed = 0.04f / (m_zoom + MAX_ZOOM + 0.1f);
        Vector3f translation = move_speed * inverse(Matrix3f(MODEL * m_arcball.matrix())) * (rel[0] * VIEW_RIGHT + rel[1] * VIEW_UP);
        m_translation += translation;
        return true;
    }
    return false;
}

bool BSDFCanvas::mouse_button_event(const Vector2i& p, int button, bool down, int modifiers) {
    if (GLCanvas::mouse_button_event(p, button, down, modifiers))
        return true;
    if (!focused() && !down)
        return false;

    // Whenever we click on the canvas, we request focus (no matter the button)
    if (down)
        request_focus();

    if (modifiers & SYSTEM_COMMAND_MOD && button == GLFW_MOUSE_BUTTON_1 && down)
    {
        m_update_incident_angle_callback(get_incident_angle(p));
        return true;
    }

    if (button == rotation_mouse_button(false))
    {
        m_arcball.button(p, down);
        return true;
    }
    else if (button == selection_mouse_button(false))
    {
        if (!down && m_selected_dataset)
        {
            Matrix4f model = model_matrix();
            Matrix4f proj = projection_matrix();

            Matrix4f mvp = proj * VIEW * model;

            SelectionBox selection_box = get_selection_box();
            
            SelectionMode                       mode = SelectionMode::STANDARD;
            if (modifiers & GLFW_MOD_SHIFT)     mode = SelectionMode::ADD;
            else if (modifiers & GLFW_MOD_ALT)  mode = SelectionMode::SUBTRACT;

            m_select_callback(mvp, selection_box, m_size, mode);
            m_selection_region = make_pair(Vector2i(0, 0), Vector2i(0, 0));
        }
        else
        {
            m_selection_region = make_pair(p, p);
        }
        return true;
    }
    return false;
}

Vector2f BSDFCanvas::get_incident_angle(const Vector2i &p)
{
    Vector2f cam_dims = m_ortho_mode ? get_ortho_dims() : get_frustum_dims();
    Vector2f relP = (Vector2f(p << 1) / m_size - Vector2f(1.0f)) * cam_dims;

    Vector4f oray;
    Vector4f dray;

    if (m_ortho_mode)
    {
        oray = Vector4f(relP.x(), relP.y(), 0.0f, 1.0f);
        dray = Vector4f(0.0f, 0.0f, 1.0f, 0.0f);
    } else {
        oray = Vector4f(0.0f, 0.0f, 0.0f, 1.0f);
        dray = Vector4f(relP.x(), relP.y(), -NEAR, 0.0f);
    }

    Matrix4f inv_model = enoki::inverse(VIEW * model_matrix());

    oray = inv_model * oray;
    dray = inv_model * dray;

    float t = -oray.z() / dray.z();
    Vector4f incident_pos = oray + dray * t;

    Vector2f i = Vector2f(incident_pos.x(), -incident_pos.y());
    float theta = enoki::norm(i) * 90.0f;
    theta = std::min(theta, 80.0f);
    float phi = TO_DEG(atan2(i.y(), i.x()));

    return Vector2f(theta, phi);
} 


bool BSDFCanvas::scroll_event(const Vector2i& p, const Vector2f& rel)
{
    if (!GLCanvas::scroll_event(p, rel))
    {
        m_zoom += rel[1]* 0.2f;
        m_zoom = std::min(MAX_ZOOM, std::max(MIN_ZOOM, m_zoom));
    }
    return true;
}

void BSDFCanvas::draw(NVGcontext* ctx)
{
    GLCanvas::draw(ctx);

    Matrix4f model = model_matrix();
    Matrix4f proj = projection_matrix();

    m_grid.draw(ctx, m_size, proj * VIEW * model);

    // draw selection region
    SelectionBox selection_box = get_selection_box();
    nvgBeginPath(ctx);
    nvgRect(ctx, selection_box.top_left.x(), selection_box.top_left.y(),
        selection_box.size.x(), selection_box.size.y());
    nvgStrokeColor(ctx, Color(1.0f, 1.0f));
    nvgStroke(ctx);
    nvgFillColor(ctx, Color(1.0f, 0.1f));
    nvgFill(ctx);
}

void BSDFCanvas::draw_gl() {
    Matrix4f model = model_matrix();
    Matrix4f proj = projection_matrix();
    Matrix4f mvp = proj * VIEW * model;

    float point_size_factor = screen()->pixel_ratio() * (m_zoom - MIN_ZOOM) / (MAX_ZOOM - MIN_ZOOM);
    for (const auto& dataset: m_datasets_to_draw)
        dataset->draw_gl(model, mvp, m_draw_flags, point_size_factor * point_size_factor * m_point_size_scale, m_color_map);

    m_grid.draw_gl(mvp);
}

void BSDFCanvas::select_dataset(shared_ptr<Dataset> dataset) {
    m_selected_dataset = dataset;
}

void BSDFCanvas::add_dataset(shared_ptr<Dataset> dataset)
{
    if (find(m_datasets_to_draw.begin(), m_datasets_to_draw.end(), dataset) == m_datasets_to_draw.end())
    {
        m_datasets_to_draw.push_back(dataset);
    }
}
void BSDFCanvas::remove_dataset(shared_ptr<Dataset> dataset)
{
    auto dataset_to_erase = find(m_datasets_to_draw.begin(), m_datasets_to_draw.end(), dataset);
    if (dataset_to_erase != m_datasets_to_draw.end())
    {
        m_datasets_to_draw.erase(dataset_to_erase);
    }
}

void BSDFCanvas::snap_to_selection_center()
{
    m_translation = m_selected_dataset ?
                        Matrix3f(inverse(MODEL)) * m_selected_dataset->selection_center() * Vector3f{-1.0f, 1.0f, 1.0f} :
                        Vector3f{ 0.0f };
}

void BSDFCanvas::set_view_angle(ViewAngles view_angle)
{
    float dir = 0.0f;
    switch (view_angle)
    {
    case UP:
        dir = (float)M_PI;
    case DOWN:
        m_arcball.set_state(enoki::rotate<Quaternion4f>(Vector3f(1, 0, 0), -M_PI* 0.5f + dir));
        break;
    case LEFT:
        dir = (float)M_PI;
    case RIGHT:
        m_arcball.set_state(enoki::rotate<Quaternion4f>(Vector3f(0, 1, 0), M_PI* 0.5f + dir));
        break;
    case BACK:
        dir = (float)M_PI;
    case FRONT:
        m_arcball.set_state(enoki::rotate<Quaternion4f>(Vector3f(0, 1, 0), dir));
        break;
    }
}

Vector2f BSDFCanvas::get_frustum_dims() const {
    float zoom_factor = m_zoom * 0.05f + 0.51f;
    float size_ratio = (float)m_size.x() / (float)m_size.y();
    const float view_angle = 46.0f - zoom_factor * 45.0f;
    float f_h = tan(view_angle / 180.0f * M_PI) * NEAR;
    float f_w = f_h * size_ratio;
    return Vector2f(f_w, f_h);
}

Vector2f BSDFCanvas::get_ortho_dims() const
{
    float zoom_factor = 1.02f - m_zoom * 0.1f;
    float size_ratio = (float)m_size.x() / (float)m_size.y();
    return Vector2f(zoom_factor * size_ratio, zoom_factor);
}

Matrix4f BSDFCanvas::model_matrix() const
{
    return m_arcball.matrix() * enoki::translate<Matrix4f>(m_translation) * MODEL;
}


Matrix4f BSDFCanvas::projection_matrix() const
{
    if (m_ortho_mode)
    {
        Vector2f ortho_dims = get_ortho_dims();
        return enoki::ortho<Matrix4f>(
            -ortho_dims.x(), ortho_dims.x(),
            -ortho_dims.y(), ortho_dims.y(),
            NEAR, FAR);
    }
    else {
        Vector2f frustum_dims = get_frustum_dims();
        return enoki::frustum<Matrix4f>(
            -frustum_dims.x(), frustum_dims.x(),
            -frustum_dims.y(), frustum_dims.y(),
            NEAR, FAR);
    }
}

SelectionBox BSDFCanvas::get_selection_box() const
{
    return {enoki::min(m_selection_region.first, m_selection_region.second),
            enoki::abs(m_selection_region.first - m_selection_region.second)};
}

TEKARI_NAMESPACE_END
