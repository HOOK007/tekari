#pragma once

#include <tekari/common.h>
#include <nanogui/glcanvas.h>
#include <nanogui/widget.h>
#include <nanogui/label.h>
#include <nanogui/window.h>

#include <tekari/axis.h>
#include <tekari/radial_grid.h>
#include <tekari/selections.h>
#include <tekari/dataset.h>

TEKARI_NAMESPACE_BEGIN

class BSDFCanvas : public nanogui::GLCanvas
{
public:
    // usefull types
    enum ViewAngles
    {
        FRONT, BACK, UP, DOWN, LEFT, RIGHT
    };
    enum MouseMode
    {
        ROTATE,
        TRANSLATE,
        SELECT,
        MOUSE_MODE_COUNT
    };

private:
    // view state constants
    static const Vector3f VIEW_ORIGIN;
    static const Vector3f VIEW_UP;
    static const Vector3f VIEW_RIGHT;
    static const Vector3f VIEW_FORWARD;
    static const Matrix4f VIEW;
    static const Matrix4f MODEL;

    static const float NEAR, FAR;

    static const int BUTTON_MAPPINGS[2][BSDFCanvas::MOUSE_MODE_COUNT];

public:
    BSDFCanvas(nanogui::Widget* parent);

    // nanogui specific methods
    virtual bool mouse_motion_event(const Vector2i& p,
                                  const Vector2i& rel,
                                  int button, int modifiers) override;
    virtual bool mouse_button_event(const Vector2i& p, int button, bool down, int modifiers) override;
    virtual bool scroll_event(const Vector2i& p, const Vector2f& rel) override;
    virtual void perform_layout(NVGcontext*) override { m_arcball.set_size(m_size); }
    virtual void draw(NVGcontext* ctx) override;
    virtual void draw_gl() override;

    // dataset addition/removale/selection
    void select_dataset(std::shared_ptr<Dataset> dataset);
    void add_dataset(std::shared_ptr<Dataset> dataset);
    void remove_dataset(std::shared_ptr<Dataset> dataset);

    void snap_to_selection_center();


    void set_view_angle(ViewAngles view_angle);
    void set_ortho_mode(bool ortho_mode) { m_ortho_mode = ortho_mode; }
    void set_selection_callback(function<void(const Matrix4f&, const SelectionBox&,
        const Vector2i&, SelectionMode)> callback) { m_select_callback = callback; }
    void set_update_incident_angle_callback(function<void(const Vector2f&)> callback) { m_update_incident_angle_callback = callback; }

    // Setters/Getters
    const RadialGrid& grid() const  { return m_grid; }
    RadialGrid& grid()              { return m_grid; }

    int draw_flags() const                      { return m_draw_flags; }
    void set_draw_flags(int flags)              { m_draw_flags = flags; }
    void set_draw_flag(int flag, bool state)    { m_draw_flags = state ? m_draw_flags | flag : m_draw_flags & ~flag; }

    void set_color_map(std::shared_ptr<ColorMap> color_map) { m_color_map = color_map; }
    const std::shared_ptr<const ColorMap> color_map() const { return m_color_map; }

    float point_size_scale() const { return m_point_size_scale; }
    void set_point_size_scale(float point_size_scale) { m_point_size_scale = point_size_scale; }

    void set_mouse_mode(MouseMode mode){ m_mouse_mode = mode; }
    MouseMode mouse_mode() const       { return m_mouse_mode; }

private:
    SelectionBox get_selection_box() const;
    Matrix4f projection_matrix() const;
    Matrix4f model_matrix() const;

    inline int rotation_mouse_button(bool dragging)     const { return BUTTON_MAPPINGS[dragging][m_mouse_mode]; }
    inline int translation_mouse_button(bool dragging)  const { return BUTTON_MAPPINGS[dragging][(m_mouse_mode + 2) % MOUSE_MODE_COUNT]; }
    inline int selection_mouse_button(bool dragging)    const { return BUTTON_MAPPINGS[dragging][(m_mouse_mode + 1) % MOUSE_MODE_COUNT]; }

    // camera getters
    Vector2f get_frustum_dims() const;
    Vector2f get_ortho_dims() const;
    Vector2f get_incident_angle(const Vector2i &p); 

    // datasets
    vector<std::shared_ptr<Dataset>>    m_datasets_to_draw;
    std::shared_ptr<Dataset>            m_selected_dataset;

    RadialGrid          m_grid;
    nanogui::Arcball    m_arcball;

    // view state
    Vector3f m_translation;
    float m_zoom;
    float m_point_size_scale;
    bool m_ortho_mode;
    MouseMode m_mouse_mode;

    // selection
    std::pair<Vector2i, Vector2i> m_selection_region;
    function<void(const Matrix4f&, const SelectionBox&,
        const Vector2i&, SelectionMode)> m_select_callback;
    function<void(const Vector2f&)> m_update_incident_angle_callback;

    // global state for sample display
    int m_draw_flags;
    std::shared_ptr<ColorMap> m_color_map;
};

TEKARI_NAMESPACE_END
