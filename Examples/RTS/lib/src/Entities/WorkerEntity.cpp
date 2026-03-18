#include "WorkerEntity.hpp"

#include <GL/gl.h>

#include "Camera.hpp"
#include "GL/VertexArray.hpp"
#include "Global/Types/AABB.hpp"
#include "World.hpp"
Worker::Worker(EntityID _ID)
	: Entity(_ID),
	  vao(),
	  vbo(cubeVertices.size() * sizeof(vec3), GL_STATIC_DRAW, cubeVertices.data()),
	  ebo(cubeIndices)
{
	shader.AddShader(vertexShaderSrc, GL_VERTEX_SHADER);
	shader.AddShader(fragmentShaderSrc, GL_FRAGMENT_SHADER);
	shader.LinkProgram();
	vao.Bind();
	vbo.Bind();
	ebo.Bind();
	vao.EnableAttribute(0);
	vao.SetAttribute(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	vao.Unbind();
	SetCollider(AABB3f(vec3(-1), vec3(1)));
}
void Worker::Render()
{
	glEnable(GL_DEPTH_TEST);
	ASSERT(Camera::GetMainCameraID().has_value(), "NO CAMERA ASSIGNED");
	shader.UseShader();
	shader.SetMat4(
		"MVP", World::Get().GetEntity<Camera>(Camera::GetMainCameraID().value()).GetViewProjMat() *
				   GetWorldMatrix());
	shader.SetVec3("uColor", ColorOverride.value_or(color));
	vao.Bind();
	glDrawElements(GL_TRIANGLES, cubeIndices.size(), GL_UNSIGNED_INT, nullptr);
	shader.UnuseShader();
	vao.Unbind();
}
