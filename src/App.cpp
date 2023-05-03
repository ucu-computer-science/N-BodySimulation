#include "App.h"
#include <random>

App::App()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    m_Window = std::make_unique<Window>(1280, 720, "N-Body Simulation");
    m_RenderProgram = std::make_unique<RenderProgram>("./data/shaders/shader.vert", "./data/shaders/shader.frag");
    m_ComputeProgram = std::make_unique<ComputeProgram>("./data/shaders/shader.comp");
    m_MortonCodesComputeProgram = std::make_unique<ComputeProgram>("./data/shaders/mortonCodes.comp");
    m_BuildingTreeComputeProgram = std::make_unique<ComputeProgram>("./data/shaders/buildingTree.comp");
    m_Texture = std::make_unique<Texture>("./data/textures/star.png");
    m_Camera = std::make_unique<Camera>(glm::vec3(0.0f, 0.0f, 150.0f), 75.0f, m_Window->GetAspectRation(), 0.1f, 1000.0f);
    m_Mesh = std::make_unique<Mesh>(c_NumberParticlesSqrt * c_NumberParticlesSqrt, -80, 80);
    m_Mouse = std::make_unique<Mouse>(m_Window->Get());
    m_Menu = std::make_unique<Menu>("Menu", m_Window->Get());

    Menu::EmbraceTheDarkness();
    m_Menu->AddSliderFloat("Speed", &m_SimulationSpeed, 0.1f, 5.0f);
    m_Menu->AddCheckbox("Simulation", &m_RunSim);
    m_Menu->AddCheckbox("Rotatation", &m_Rotate);
    m_Menu->AddButton("Enter FlyCam Mode", [this] {m_Mouse->DisableCursor(m_Window->Get()); });
    m_Menu->AddText("[press TAB - to exit]");
    m_Menu->AddText("['WASD' for control ]");

    std::vector<glm::vec4> data(c_NumberParticlesSqrt * c_NumberParticlesSqrt);
    std::normal_distribution<float> distX(0, 10);
    std::normal_distribution<float> distY(0, 10);
    std::normal_distribution<float> distZ(0, 10);
    std::default_random_engine eng;
    for (size_t i = 0; i < c_NumberParticlesSqrt * c_NumberParticlesSqrt; i++)
    {
        glm::vec3 pos = { distX(eng), distY(eng), distZ(eng) };
        data[i] = glm::vec4(pos.x, pos.y, pos.z, 0.0f);
    }
    m_PositionBuffers.emplace_back(std::make_unique<SSBO<glm::vec4>>(data.data(), data.size()));
    m_PositionBuffers.emplace_back(std::make_unique<SSBO<glm::vec4>>(data.data(), data.size()));
    m_VelocityBuffers.emplace_back(std::make_unique<SSBO<glm::vec4>>(data.size()));
    m_VelocityBuffers.emplace_back(std::make_unique<SSBO<glm::vec4>>(data.size()));

    m_MortonCodesBuffer = std::make_unique<SSBO<unsigned int>>(c_NumberParticlesSqrt * c_NumberParticlesSqrt);

    std::vector<TreeNode_t> treeNodesSSBO(2*(c_NumberParticlesSqrt * c_NumberParticlesSqrt) - 1);
    m_TreeNodesBuffer = std::make_unique<SSBO<TreeNode_t>>(treeNodesSSBO.data(), treeNodesSSBO.size());
}

App::~App()
{
    glfwTerminate();
}

void App::Run()
{
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);
    m_Clock.Restart();
    m_RenderProgram->Use();
    m_Time = (float)glfwGetTime();
    while (m_Window->Open())
    {
        float deltaTime = m_Clock.Stamp();

        // Processing user input
        ProcessEvents(deltaTime);

        // Rendering stuff goes here
        DoFrame(deltaTime);

        m_Menu->Render();

        PollEvents(deltaTime);
        m_Window->SwapBuffers();
    }
}

void App::DoFrame(float dt)
{
    if (m_RunSim)
    {
        // morton codes compute part
        m_MortonCodesComputeProgram->Use();
        m_MortonCodesComputeProgram->SetFloat("numberOfParticlesSqrt", c_NumberParticlesSqrt);
        m_PositionBuffers[m_FrameCounter % 2]->Bind(1);
        m_MortonCodesBuffer->Bind(5);

        m_MortonCodesComputeProgram->SetFvec3("boundingBox", m_GeneralBoundingBox);
        glDispatchCompute(c_NumberParticlesSqrt / 8, c_NumberParticlesSqrt / 4, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        m_BuildingTreeComputeProgram->Use();
        m_BuildingTreeComputeProgram->SetFloat("numberOfParticlesSqrt", c_NumberParticlesSqrt);
        m_PositionBuffers[m_FrameCounter % 2]->Bind(1);
        m_VelocityBuffers[m_FrameCounter % 2]->Bind(3);
        m_MortonCodesBuffer->Bind(5);
        m_TreeNodesBuffer->Bind(6);
        glDispatchCompute(c_NumberParticlesSqrt / 8, c_NumberParticlesSqrt / 4, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // compute part
        m_ComputeProgram->Use();
        m_ComputeProgram->SetFloat("deltaTime", dt * m_SimulationSpeed);
        m_ComputeProgram->SetFvec3("boundingBox", m_GeneralBoundingBox);
        m_ComputeProgram->SetFloat("numberOfParticlesSqrt", c_NumberParticlesSqrt);

        m_PositionBuffers[m_FrameCounter % 2]       ->Bind(1);
        m_PositionBuffers[(m_FrameCounter + 1) % 2] ->Bind(2);
        m_VelocityBuffers[m_FrameCounter % 2]       ->Bind(3);
        m_VelocityBuffers[(m_FrameCounter + 1) % 2] ->Bind(4);


        glDispatchCompute(c_NumberParticlesSqrt / 8, c_NumberParticlesSqrt / 4, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    // render part
    int bufferIndex = (m_RunSim ? m_FrameCounter : 0);
    m_RenderProgram->Use();

    m_Texture->Bind(0);

    m_PositionBuffers[bufferIndex % 2]      ->Bind(1);
    m_PositionBuffers[(bufferIndex + 1) % 2]->Bind(2);
    m_VelocityBuffers[bufferIndex % 2]      ->Bind(3);
    m_VelocityBuffers[(bufferIndex + 1) % 2]->Bind(4);

    m_Time = (m_Rotate ? m_Time + dt : m_Time);
    auto model = glm::rotate(glm::identity<glm::mat4x4>(), m_Time, glm::vec3(0, 1, 0));
    m_RenderProgram->SetMat4x4("u_Model", model);
    m_RenderProgram->SetInt("u_Texture", 0);
    m_RenderProgram->SetMat4x4("u_ProjView", m_Camera->GetProjectionMatrix() * m_Camera->GetViewMatrix());
    m_RenderProgram->SetMat4x4("u_CameraRotation", m_Camera->GetRotationMatrix());

    m_Window->Clear(0.05f, 0.05f, 0.1f);
    m_Mesh->Draw();
    m_FrameCounter++;
}

void App::ProcessEvents(float dt)
{
    if (glfwGetKey(m_Window->Get(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
        m_Window->Close();

    // Camera movement                                        
    using CamDir = Camera::Direction;
    if (glfwGetKey(m_Window->Get(), GLFW_KEY_A) == GLFW_PRESS)
        m_Camera->Move(CamDir::Left, dt);
    if (glfwGetKey(m_Window->Get(), GLFW_KEY_D) == GLFW_PRESS)
        m_Camera->Move(CamDir::Right, dt);
    if (glfwGetKey(m_Window->Get(), GLFW_KEY_W) == GLFW_PRESS)
        m_Camera->Move(CamDir::Front, dt);
    if (glfwGetKey(m_Window->Get(), GLFW_KEY_S) == GLFW_PRESS)
        m_Camera->Move(CamDir::Back, dt);
    if (glfwGetKey(m_Window->Get(), GLFW_KEY_SPACE) == GLFW_PRESS)
        m_Camera->Move(CamDir::Up, dt);
    if (glfwGetKey(m_Window->Get(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        m_Camera->Move(CamDir::Down, dt);
    if (glfwGetKey(m_Window->Get(), GLFW_KEY_TAB) == GLFW_PRESS)
        m_Mouse->EnableCursor(m_Window->Get());

    // Camera rotation
    glm::vec2 mouseOffset = m_Mouse->GetOffset(m_Window->Get());
    const float sensitivity = 0.03f;
    mouseOffset *= sensitivity;
    m_Camera->ProcessMouseInput(mouseOffset);
}

void App::PollEvents(float dt)
{
    glfwPollEvents();
}
