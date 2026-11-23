// dear imgui: Graphics API Abstraction - shader services shared by the backends
// The compile log (ImPlatform_GetShaderLog), the blend presets and asynchronous program compilation
// (ImPlatform_CreateShaderProgramAsync), written over ImPlatform_CreateShader and ImPlatform_CreateShaderProgram.
// Direct3D 10, 11 and 12 compile the HLSL of a job on a worker thread (IMPLATFORM_SHADER_COMPILE_THREAD); the other
// backends compile when the job is created.

#include "ImPlatform_Internal.h"

#if IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER

#include <string>
#ifdef IMPLATFORM_SHADER_COMPILE_THREAD
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#endif

// An IM_GFX_CUSTOM application reports the messages of its own ImPlatform_CreateShader.
#if IM_CURRENT_GFX != IM_GFX_CUSTOM
IMPLATFORM_API const char* ImPlatform_GetShaderLog(void)
{
    return ImPlatform_ShaderLogBuffer().c_str();
}
#endif

IMPLATFORM_API ImPlatform_BlendDesc ImPlatform_BlendDesc_Preset(ImPlatform_BlendPreset preset)
{
    ImPlatform_BlendDesc b = ImPlatform_BlendFromKey(0);        // Straight alpha
    switch (preset)
    {
    case ImPlatform_BlendPreset_Premultiplied:
        b.src_color = ImPlatform_BlendFactor_One;
        break;
    case ImPlatform_BlendPreset_Opaque:
        b.enable = false;
        break;
    case ImPlatform_BlendPreset_Subtractive:
        b.op_color = ImPlatform_BlendOp_RevSubtract;
        // fallthrough
    case ImPlatform_BlendPreset_Additive:
        b.dst_color = ImPlatform_BlendFactor_One;
        break;
    case ImPlatform_BlendPreset_Multiplicative:
        b.src_color = ImPlatform_BlendFactor_DstColor;
        b.dst_color = ImPlatform_BlendFactor_Zero;
        break;
    case ImPlatform_BlendPreset_Screen:
        b.src_color = ImPlatform_BlendFactor_One;
        b.dst_color = ImPlatform_BlendFactor_InvSrcColor;
        break;
    case ImPlatform_BlendPreset_Darken:
        b.op_color = ImPlatform_BlendOp_Min;
        break;
    case ImPlatform_BlendPreset_Lighten:
        b.op_color = ImPlatform_BlendOp_Max;
        break;
    default:
        break;
    }
    if (preset >= ImPlatform_BlendPreset_Additive)      // These keep the target's alpha
    {
        b.src_alpha = ImPlatform_BlendFactor_Zero;
        b.dst_alpha = ImPlatform_BlendFactor_One;
    }
    return b;
}

IMPLATFORM_API bool ImPlatform_SetShaderProgramBlendPreset(ImPlatform_ShaderProgram program, ImPlatform_BlendPreset preset)
{
    const ImPlatform_BlendDesc blend = ImPlatform_BlendDesc_Preset(preset);
    return ImPlatform_SetShaderProgramBlend(program, &blend);
}

// ============================================================================
// Asynchronous program compilation
// ============================================================================

struct ImPlatform_ShaderJobData
{
    int                      stages;            // 2: a program (vertex, fragment); 1: one shader (compute)
    ImPlatform_ShaderDesc    desc[2];           // Their pointers are into the strings below
    std::string              source[2], entry[2], cache_key[2], bytecode[2];
    bool                     list_source;       // A failure's log ends with the numbered source (generated shaders)
    ImPlatform_Shader        shaders[2];
    ImPlatform_ShaderProgram program;
    ImPlatform_ShaderStatus  status;
    ImGuiTextBuffer          log;
#ifdef IMPLATFORM_SHADER_COMPILE_THREAD
    bool                     compile[2];        // The stage is HLSL source: the worker turns it into bytecode[]
    char*                    compile_log[2];    // Worker: the messages of a stage that did not compile
    std::atomic<bool>        compiled;          // Worker: both stages were processed
#endif
};

// Creates the shaders and the program of a job whose compilation has ended (graphics thread).
static void ImPlatform_ShaderJobFinish(ImPlatform_ShaderJobData* job)
{
    const char* const names[2] = { job->stages == 1 ? "compute" : "vertex", "fragment" };
    for (int i = 0; i < job->stages; i++)
    {
        ImPlatform_ShaderDesc desc = job->desc[i];
        bool failed = false;
#ifdef IMPLATFORM_SHADER_COMPILE_THREAD
        if (job->compile[i])
        {
            if (job->bytecode[i].empty())
            {
                job->log.appendf("%s shader: %s\n", names[i], job->compile_log[i] ? job->compile_log[i] : "compilation failed");
                failed = true;
            }
            desc.source_code = NULL;
            desc.bytecode = job->bytecode[i].data();
            desc.bytecode_size = (unsigned int)job->bytecode[i].size();
        }
#endif
        if (!failed)
        {
#if IM_CURRENT_GFX != IM_GFX_CUSTOM
            ImPlatform_ShaderLogBuffer().clear();
#endif
            job->shaders[i] = ImPlatform_CreateShader(&desc);
            if (!job->shaders[i])
            {
                job->log.appendf("%s shader: %s\n", names[i], ImPlatform_GetShaderLog());
                failed = true;
            }
        }
        if (failed && job->list_source && job->desc[i].source_code)
            ImPlatform_AppendNumberedSource(job->log, job->desc[i].source_code);
    }
    if (job->stages == 2 && job->shaders[0] && job->shaders[1])
    {
#if IM_CURRENT_GFX != IM_GFX_CUSTOM
        ImPlatform_ShaderLogBuffer().clear();
#endif
        job->program = ImPlatform_CreateShaderProgram(job->shaders[0], job->shaders[1]);
        if (!job->program)
            job->log.appendf("program: %s\n", ImPlatform_GetShaderLog());
    }
    const bool ready = job->stages == 1 ? job->shaders[0] != NULL : job->program != NULL;
    job->status = ready ? ImPlatform_ShaderStatus_Ready : ImPlatform_ShaderStatus_Failed;
    if (!ready)
        fprintf(stderr, "[ImPlatform] shader job failed:\n%s", job->log.c_str());
}

#ifdef IMPLATFORM_SHADER_COMPILE_THREAD
// One worker for every job, in creation order. Started by the first job; joined when the process exits.
struct ImPlatform_ShaderCompiler
{
    std::mutex                            mutex;
    std::condition_variable               wake;
    std::deque<ImPlatform_ShaderJobData*> queue;
    std::thread                           thread;
    bool                                  quit = false;

    void Run()
    {
        for (;;)
        {
            ImPlatform_ShaderJobData* job = NULL;
            {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait(lock, [this] { return quit || !queue.empty(); });
                if (quit)
                    return;
                job = queue.front();
                queue.pop_front();
            }
            for (int i = 0; i < 2; i++)
                if (job->compile[i])
                    if (ID3DBlob* blob = ImPlatform_D3DCompileShader(&job->desc[i], &job->compile_log[i]))
                    {
                        job->bytecode[i].assign((const char*)blob->GetBufferPointer(), blob->GetBufferSize());
                        blob->Release();
                    }
            job->compiled = true;
        }
    }

    ~ImPlatform_ShaderCompiler()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            quit = true;
        }
        wake.notify_all();
        if (thread.joinable())
            thread.join();
    }
};

static ImPlatform_ShaderCompiler& ImPlatform_GetShaderCompiler()
{
    static ImPlatform_ShaderCompiler compiler;
    return compiler;
}
#endif // IMPLATFORM_SHADER_COMPILE_THREAD

// fragment_desc NULL: a job for the one shader of vertex_desc (a compute shader).
ImPlatform_ShaderJob ImPlatform_ShaderJobCreate(const ImPlatform_ShaderDesc* vertex_desc, const ImPlatform_ShaderDesc* fragment_desc,
                                                bool list_source)
{
    if (!vertex_desc)
        return NULL;
    ImPlatform_ShaderJobData* job = new ImPlatform_ShaderJobData();
    job->stages = fragment_desc ? 2 : 1;
    job->list_source = list_source;
    job->status = ImPlatform_ShaderStatus_Compiling;
    const ImPlatform_ShaderDesc* in[2] = { vertex_desc, fragment_desc };
#ifdef IMPLATFORM_SHADER_COMPILE_THREAD
    bool queued = false;
#endif
    for (int i = 0; i < job->stages; i++)
    {
        // The defines are applied here: the worker allocates nothing through Dear ImGui.
        ImPlatform_ShaderDesc with;
        ImGuiTextBuffer text;
        const ImPlatform_ShaderDesc* d = ImPlatform_ShaderApplyDefines(in[i], &with, &text);
        ImPlatform_ShaderDesc& desc = job->desc[i];
        desc = *d;
        desc.defines = NULL;
        if (d->source_code)
            desc.source_code = (job->source[i] = d->source_code).c_str();
        if (d->entry_point)
            desc.entry_point = (job->entry[i] = d->entry_point).c_str();
        if (d->cache_key)
            desc.cache_key = (job->cache_key[i] = d->cache_key).c_str();
        if (d->bytecode && d->bytecode_size)
            desc.bytecode = job->bytecode[i].assign((const char*)d->bytecode, d->bytecode_size).data();
#ifdef IMPLATFORM_SHADER_COMPILE_THREAD
        job->compile[i] = desc.source_code && !desc.bytecode && desc.format == ImPlatform_ShaderFormat_HLSL;
        queued |= job->compile[i];
#endif
    }
#ifdef IMPLATFORM_SHADER_COMPILE_THREAD
    if (queued)
    {
        ImPlatform_ShaderCompiler& compiler = ImPlatform_GetShaderCompiler();
        {
            std::lock_guard<std::mutex> lock(compiler.mutex);
            compiler.queue.push_back(job);
            if (!compiler.thread.joinable())
                compiler.thread = std::thread([&compiler] { compiler.Run(); });
        }
        compiler.wake.notify_one();
        return job;
    }
#endif
    ImPlatform_ShaderJobFinish(job);
    return job;
}

IMPLATFORM_API ImPlatform_ShaderJob ImPlatform_CreateShaderProgramAsync(const ImPlatform_ShaderDesc* vertex_desc,
                                                                       const ImPlatform_ShaderDesc* fragment_desc)
{
    return vertex_desc && fragment_desc ? ImPlatform_ShaderJobCreate(vertex_desc, fragment_desc, false) : NULL;
}

#if IMPLATFORM_GFX_SUPPORT_COMPUTE
IMPLATFORM_API ImPlatform_ShaderJob ImPlatform_CreateComputeShaderAsync(const ImPlatform_ShaderDesc* desc)
{
    return desc && desc->stage == ImPlatform_ShaderStage_Compute ? ImPlatform_ShaderJobCreate(desc, NULL, false) : NULL;
}

IMPLATFORM_API ImPlatform_Shader ImPlatform_GetShaderJobShader(ImPlatform_ShaderJob job_handle)
{
    ImPlatform_ShaderJobData* job = (ImPlatform_ShaderJobData*)job_handle;
    return job && job->stages == 1 && ImPlatform_GetShaderJobStatus(job, NULL) == ImPlatform_ShaderStatus_Ready ? job->shaders[0] : NULL;
}
#endif

IMPLATFORM_API ImPlatform_ShaderStatus ImPlatform_GetShaderJobStatus(ImPlatform_ShaderJob job_handle, ImPlatform_ShaderProgram* out_program)
{
    ImPlatform_ShaderJobData* job = (ImPlatform_ShaderJobData*)job_handle;
#ifdef IMPLATFORM_SHADER_COMPILE_THREAD
    if (job && job->status == ImPlatform_ShaderStatus_Compiling && job->compiled)
        ImPlatform_ShaderJobFinish(job);
#endif
    if (out_program)
        *out_program = job ? job->program : NULL;
    return job ? job->status : ImPlatform_ShaderStatus_Failed;
}

IMPLATFORM_API const char* ImPlatform_GetShaderJobLog(ImPlatform_ShaderJob job)
{
    return job ? ((ImPlatform_ShaderJobData*)job)->log.c_str() : "";
}

IMPLATFORM_API void ImPlatform_DestroyShaderJob(ImPlatform_ShaderJob job_handle)
{
    ImPlatform_ShaderJobData* job = (ImPlatform_ShaderJobData*)job_handle;
    if (!job)
        return;
#ifdef IMPLATFORM_SHADER_COMPILE_THREAD
    if (job->status == ImPlatform_ShaderStatus_Compiling)
    {
        // Still queued: the worker never sees it. Otherwise the worker is compiling it: wait for its last stage.
        ImPlatform_ShaderCompiler& compiler = ImPlatform_GetShaderCompiler();
        bool queued = false;
        {
            std::lock_guard<std::mutex> lock(compiler.mutex);
            for (auto it = compiler.queue.begin(); it != compiler.queue.end(); ++it)
                if (*it == job)
                {
                    compiler.queue.erase(it);
                    queued = true;
                    break;
                }
        }
        while (!queued && !job->compiled)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    for (char* messages : job->compile_log)
        free(messages);
#endif
    if (job->program)
        ImPlatform_DestroyShaderProgram(job->program);
    for (ImPlatform_Shader shader : job->shaders)
        if (shader)
            ImPlatform_DestroyShader(shader);
    delete job;
}

#endif // IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
