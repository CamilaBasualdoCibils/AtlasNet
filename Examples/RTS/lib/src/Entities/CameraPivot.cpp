#include "CameraPivot.hpp"

#include "Entities/Camera.hpp"
#include "Window.hpp"
#include "World.hpp"
#include "imgui.h"
void CameraPivot::Update(float deltaTime)
{
	auto& window = Window::Get();
	glm::vec2 mouseNDC = window.GetMousePosNDC();  // x,y in [-1,1]

	// Right mouse button check
	if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
	{
		if (!bDragging)
		{
			bDragging = true;
			lastMouseNDC = mouseNDC;
		}
		else
		{
			glm::vec2 deltaNDC = mouseNDC - lastMouseNDC;
			lastMouseNDC = mouseNDC;

			if (auto camID = Camera::GetMainCameraID(); camID.has_value())
			{
				auto& cam = World::Get().GetEntity<Camera>(camID.value());

				// Only for orthographic cameras

				float aspect = Window::Get().GetWindowAspect();
				float worldHalfWidth = cam.GetOrthoSensorSize() * aspect;
				float worldHalfHeight = cam.GetOrthoSensorSize();

				glm::mat4 view = cam.GetViewMat();

				// Camera axes in world space
				glm::vec3 camRight = glm::normalize(glm::vec3(view[0][0], view[1][0], view[2][0]));
				glm::vec3 camUp = glm::normalize(glm::vec3(view[0][1], view[1][1], view[2][1]));
				glm::vec3 camForward = glm::normalize(
					glm::vec3(-view[0][2], -view[1][2], -view[2][2]));	// OpenGL convention

				// Apply drag in camera plane
				glm::vec3 move =
					-deltaNDC.x * worldHalfWidth * camRight - deltaNDC.y * worldHalfHeight * camUp;

				transform.Pos += move;

				// Project onto XZ plane along camera forward direction
				// Compute the Y offset along camera forward
				float t = (0.0f - transform.Pos.y) / camForward.y;	// intersect Y=0 plane
				transform.Pos += camForward * t;

				// Optional: keep Y exactly zero
				transform.Pos.y = 0.0f;
			}
		}
	}
	else
	{
		bDragging = false;
	}
}
