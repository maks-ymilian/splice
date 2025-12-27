
 bool ImGui_ImplOpenGL3_Init(const char* glsl_version = nullptr);
 void ImGui_ImplOpenGL3_Shutdown();
 void ImGui_ImplOpenGL3_NewFrame();
 void ImGui_ImplOpenGL3_RenderDrawData(ImDrawData* draw_data);
 bool ImGui_ImplOpenGL3_CreateDeviceObjects();
 void ImGui_ImplOpenGL3_DestroyDeviceObjects();
 void ImGui_ImplOpenGL3_UpdateTexture(ImTextureData* tex);