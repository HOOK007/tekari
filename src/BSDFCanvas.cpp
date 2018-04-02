#include "BSDFCanvas.h"

BSDFCanvas::BSDFCanvas(Widget *parent)
:   nanogui::GLCanvas(parent)
,   m_ViewOrigin(0, 2, 4)
,   m_ViewTarget(0, 0, 0)
,   m_ViewUp(0, 1, 0)
,	m_Zoom(0)
,	m_OrthoMode(false)
{}

bool BSDFCanvas::mouseMotionEvent(const nanogui::Vector2i &p,
                              const nanogui::Vector2i &rel,
                              int button, int modifiers) {
    if (!GLCanvas::mouseMotionEvent(p, rel, button, modifiers))
    {
        if (button == GLFW_MOUSE_BUTTON_2)
        {
            m_Arcball.motion(p);
            return true;
        }
        else if (button == GLFW_MOUSE_BUTTON_5)
        {
            m_ViewTarget += nanogui::Vector3f(-rel[0] * 0.01f, 0, -rel[1] * 0.01f);
            return true;
        }
        return false;
    }
    return true;
}

bool BSDFCanvas::mouseButtonEvent(const nanogui::Vector2i &p, int button, bool down, int modifiers) {
    if (!GLCanvas::mouseButtonEvent(p, button, down, modifiers))
    {
        if (button == GLFW_MOUSE_BUTTON_1)
        {
            m_Arcball.button(p, down);
            return true;
        }
        return false;
    }
	return true;
}

bool BSDFCanvas::keyboardEvent(int key, int scancode, int action, int modifiers)
{
	if (!GLCanvas::keyboardEvent(key, scancode, action, modifiers))
	{
		if (action == GLFW_PRESS)
		{
			switch (key)
			{
			case GLFW_KEY_KP_5:
				m_OrthoMode = !m_OrthoMode;
				return true;
			case GLFW_KEY_KP_1:
			case GLFW_KEY_KP_3:
				m_Arcball.setState(nanogui::Quaternionf::Identity());
				m_ViewOrigin = key == GLFW_KEY_KP_1 ?	nanogui::Vector3f(0.0f, 0.0f, modifiers == GLFW_MOD_CONTROL ? -4.0f : 4.0f) :
														nanogui::Vector3f(modifiers == GLFW_MOD_CONTROL ? -4.0f : 4.0f, 0.0f, 0.0f);
				m_ViewTarget = nanogui::Vector3f(0.0f, 0.0f, 0.0f);
				m_ViewUp = nanogui::Vector3f(0.0f, 1.0f, 0.0f);
				return true;
			case GLFW_KEY_KP_7:
				m_Arcball.setState(nanogui::Quaternionf::Identity());
				m_ViewOrigin = nanogui::Vector3f(0.0f, modifiers == GLFW_MOD_CONTROL ? -4.0f : 4.0f, 0.0f);
				m_ViewTarget = nanogui::Vector3f(0.0f, 0.0f, 0.0f);
				m_ViewUp = nanogui::Vector3f(0.0f, 0.0f, 1.0f);
				return true;
			}
		}
		return false;
	}
	return true;
}

bool BSDFCanvas::scrollEvent(const nanogui::Vector2i &p, const nanogui::Vector2f &rel)
{
    if (!GLCanvas::scrollEvent(p, rel))
    {
		m_Zoom += rel[1] * 0.2f;
		m_Zoom = std::min(10.0f, std::max(-10.0f, m_Zoom));
    }
    return true;
}

void BSDFCanvas::drawGL() {
    using namespace nanogui;

    Matrix4f view, proj;
	view = lookAt(m_ViewOrigin, m_ViewTarget, m_ViewUp);
	float near = 0.01f, far = 100.0f;
	float zoomFactor = (m_Zoom + 10.0f) / 20.0f + 0.01f;
	float sizeRatio = (float)mSize.x() / (float)mSize.y();
	if (m_OrthoMode)
	{
		zoomFactor = (1.02f - zoomFactor) * 2.0f;
		proj = ortho(-zoomFactor * sizeRatio, zoomFactor * sizeRatio,
					 -zoomFactor, zoomFactor,
					 near, far);
	}
	else {
		const float viewAngle = 81.0f - zoomFactor * 80.0f;
		float fH = std::tan(viewAngle / 360.0f * M_PI) * near;
		float fW = fH * sizeRatio;
		proj = frustum(-fW, fW, -fH, fH, near, far);
	}

	for (const auto& dataSample: m_DataSamplesToDraw)
	{
		dataSample->draw(m_Arcball.matrix(), view, proj);
	}
    m_Grid.draw(m_Arcball.matrix(), view, proj);
}

void BSDFCanvas::addDataSample(std::shared_ptr<DataSample> dataSample)
{
	if (std::find(m_DataSamplesToDraw.begin(), m_DataSamplesToDraw.end(), dataSample) == m_DataSamplesToDraw.end())
	{
		m_DataSamplesToDraw.push_back(dataSample);
	}
}
void BSDFCanvas::removeDataSample(std::shared_ptr<DataSample> dataSample)
{
	auto dataSampleToErase = std::find(m_DataSamplesToDraw.begin(), m_DataSamplesToDraw.end(), dataSample);
	if (dataSampleToErase != m_DataSamplesToDraw.end())
	{
		m_DataSamplesToDraw.erase(dataSampleToErase);
	}
}
