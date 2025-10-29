#include "Cartograph.hpp"
#include "PartitionVisualization.hpp"
#include "Cartograph.hpp"
#include "EmbedResources.hpp"
#include "Interlink/Interlink.hpp"
void Cartograph::Startup()
{
	// glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	_glfwwindow = glfwCreateWindow(1920, 1080, "Cartograph", nullptr, nullptr);
	glfwMakeContextCurrent(_glfwwindow);
	glewInit();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImPlot3D::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	(void)io;
	ImFontConfig fontconfig;
	fontconfig.SizePixels = 20.0f;
	io.Fonts->AddFontDefault(&fontconfig);
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui_ImplGlfw_InitForOpenGL(_glfwwindow, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	// Interlink::Get().Init(InterlinkProperties{.ThisID = InterLinkIdentifier::MakeIDCartograph(), .logger = logger, .callbacks = InterlinkCallbacks{}});
}
void Cartograph::DrawBackground()
{

	// Remove window padding and borders
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
									ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
									ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
									ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

	const ImGuiViewport *viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	ImGui::Begin("MainDockspace", nullptr, window_flags);
	ImGui::PopStyleVar(2);

	// Create the dockspace
	ImGuiID dockspace_id = ImGui::GetID("MyDockspace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

	// Draw background image behind everything in this window
	ImDrawList *draw_list = ImGui::GetWindowDrawList();
	ImVec2 win_pos = ImGui::GetWindowPos();
	ImVec2 win_size = ImGui::GetWindowSize();

	// Get center of the window
	ImVec2 center = ImVec2(win_pos.x + win_size.x * 0.5f, win_pos.y + win_size.y * 0.5f);

	// Assuming the image has known size
	ImVec2 image_size(512, 512);
	float BorderOffset = 10;
	float alpha = 80;
	ImVec2 image_min(center.x - (image_size.x * 0.5f), center.y - (image_size.y * 0.5f));
	ImVec2 image_max(center.x + (image_size.x * 0.5f), center.y + (image_size.y * 0.5f));
	ImVec2 border_min(center.x - (image_size.x * 0.5f + BorderOffset), center.y - (image_size.y * 0.5f + BorderOffset));
	ImVec2 border_max(center.x + (image_size.x * 0.5f + BorderOffset), center.y + (image_size.y * 0.5f + BorderOffset));
	static auto backgroundImage = LoadTextureFromCompressed(EmbedResources::map_search_png, EmbedResources::map_search_size);
	// Draw it slightly transparent
	draw_list->AddImage((ImTextureID)backgroundImage->GetHandle(), image_min, image_max, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, alpha));
	draw_list->AddRect(border_min, border_max, IM_COL32(255, 255, 255, alpha), BorderOffset, 0, 2.0f);
	ImGui::End();
}
void Cartograph::DrawConnectTo()
{
	ImGui::Begin("ConnectTo", nullptr, ImGuiWindowFlags_NoTitleBar);

	ImGui::InputTextWithHint("##connect", "IP address of God", &IP2God);
	ImGui::SameLine();
	if (ImGui::Button("Connect"))
	{
	}
	ImGui::End();
}
void Cartograph::DrawMap()
{
	ImGui::Begin("Map", nullptr, ImGuiWindowFlags_NoTitleBar);
	ImPlot::BeginPlot("Map View", ImVec2(-1, -1), ImPlotFlags_CanvasOnly);

	ImPlot::EndPlot();
	ImGui::End();
}
void Cartograph::DrawPartitionGrid()
{
	ImGui::Begin("Partitions");

	ImGui::End();
}
void Cartograph::DrawLog()
{
	ImGui::Begin("Log");
	ImGui::End();
}
void Cartograph::Update()
{
}
void Cartograph::Render()
{
	DrawBackground();
	DrawConnectTo();
	DrawMap();
	DrawPartitionGrid();
	DrawLog();
}
std::shared_ptr<Texture> Cartograph::LoadTextureFromCompressed(const void *data, size_t size)
{
	std::shared_ptr<Texture> tex = std::make_shared<Texture>();
	ivec2 Dimensions;
	int channelCount;
	void *imageData = stbi_load_from_memory((const uint8 *)data, size, &Dimensions.x, &Dimensions.y, &channelCount, 4);
	tex->Alloc2D(Dimensions, eRGBA, GL_RGBA8_SNORM, imageData);
	tex->GenerateMipmaps();
	stbi_image_free(imageData);
	return tex;
}
void Cartograph::Run()
{
	Startup();
	while (!glfwWindowShouldClose(_glfwwindow))
	{
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		Update();
		Render();

		ImGui::ShowDemoWindow();
		ImPlot::ShowDemoWindow();
		ImPlot3D::ShowDemoWindow();

		ImGui::EndFrame();
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(_glfwwindow);
		glfwPollEvents();
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			GLFWwindow *backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
	}
}