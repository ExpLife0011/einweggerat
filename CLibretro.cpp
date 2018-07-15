#include "stdafx.h"
#include <windows.h>
#include "CLibretro.h"
#include "libretro.h"
#include "io/gl_render.h"
#include "gui/utf8conv.h"
#define INI_IMPLEMENTATION
#define INI_STRNICMP( s1, s2, cnt ) (strcmp( s1, s2) )
#include "ini.h"
#include <algorithm>
#include <numeric>  
using namespace std;
using namespace utf8util;
#include <sys/stat.h>
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#include <dwmapi.h>
#pragma comment (lib,"dwmapi.lib")

#define INLINE 

static struct {
  HMODULE handle;
  bool initialized;
  void(*retro_init)(void);
  void(*retro_deinit)(void);
  unsigned(*retro_api_version)(void);
  void(*retro_get_system_info)(struct retro_system_info *info);
  void(*retro_get_system_av_info)(struct retro_system_av_info *info);
  void(*retro_set_controller_port_device)(unsigned port, unsigned device);
  void(*retro_reset)(void);
  void(*retro_run)(void);
  size_t(*retro_serialize_size)(void);
  bool(*retro_serialize)(void *data, size_t size);
  bool(*retro_unserialize)(const void *data, size_t size);
  bool(*retro_load_game)(const struct retro_game_info *game);
  void *(*retro_get_memory_data)(unsigned id);
  size_t(*retro_get_memory_size)(unsigned id);
  void(*retro_unload_game)(void);
} g_retro;

static void core_unload() {
  if (g_retro.initialized)
    g_retro.retro_deinit();
  if (g_retro.handle)
  {
    FreeLibrary(g_retro.handle);
    g_retro.handle = NULL;
  }
}

static void core_log(enum retro_log_level level, const char *fmt, ...) {
  char buffer[4096] = { 0 };
  char buffer2[4096] = { 0 };
  static const char * levelstr[] = { "dbg", "inf", "wrn", "err" };
  va_list va;
  va_start(va, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, va);
  va_end(va);
  if (level == 0)
    return;
  sprintf(buffer2, "[%s] %s", levelstr[level], buffer);
  fprintf(stdout, "%s", buffer2);
}


uintptr_t core_get_current_framebuffer() {
  return g_video.fbo_id;
}

#pragma optimize( "", off )  
void init_coresettings(retro_variable *var)
{
   CLibretro * retro = CLibretro::GetSingleton();
   FILE *fp = NULL;
   std::vector<CLibretro::core_vars> variables1;
   variables1.clear();
   //set up core variable information and default key settings
   while (var != NULL && var->key != NULL)
   {
      CLibretro::core_vars vars_struct = { 0 };
      strcpy(vars_struct.name, var->key);
      char descript[50] = { 0 };
      char *e = strchr((char*)var->value, ';');
      strncpy(descript, var->value, (int)(e - (char*)var->value));
      strcpy(vars_struct.description, descript);
      char * pch = strstr((char*)var->value, (char*)"; ");
      pch += strlen("; ");
      strcpy(vars_struct.usevars, pch);
      char* str2 = strstr(pch, (char*)"|");
      if (!str2){
          strcpy(vars_struct.var, pch);
      }
      else {
      strncpy(vars_struct.var, pch, str2 - pch);
      }
      variables1.push_back(vars_struct);
      ++var;
   }

  fp = _wfopen(retro->corevar_path, L"r");
  if (!fp)
  {
    create_filez:
    //create a new file with defaults
    ini_t* ini = ini_create(NULL);
    for (int i = 0; i < variables1.size(); i++)
    {
       ini_property_add(ini, INI_GLOBAL_SECTION, (char*)variables1[i].name,strlen(variables1[i].name), (char*)variables1[i].var, strlen(variables1[i].var));
       retro->variables.push_back(variables1[i]);
    }
    int size = ini_save(ini, NULL, 0); // Find the size needed
    char* data = (char*)malloc(size);
    size = ini_save(ini, data, size); // Actually save the file
    ini_destroy(ini);
    fp = _wfopen(retro->corevar_path, L"w");
    fwrite(data, 1, size, fp);
    fclose(fp);
    free(data);
  }
  else
  {
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char* data = (char*)malloc(size + 1);
    fread(data, 1, size, fp);
    data[size] = '\0';
    fclose(fp);
    ini_t* ini = ini_load(data, NULL);
    free(data);
    int vars_num1 = variables1.size();
    int vars_infile = ini_property_count(ini, INI_GLOBAL_SECTION);
    if (vars_infile != vars_num1) {
       fclose(fp);
    }
    bool save = false;
    for (int i= 0; i < vars_num1; i++)
    {
       int second_index = ini_find_property(ini, INI_GLOBAL_SECTION, (char*)variables1[i].name, strlen(variables1[i].name));
       if (second_index != INI_NOT_FOUND)
       {
          const char* variable_val = ini_property_value(ini, INI_GLOBAL_SECTION, second_index);
          strcpy(variables1[i].var, variable_val);
          retro->variables.push_back(variables1[i]);
       }
       else
       {
          ini_property_add(ini, INI_GLOBAL_SECTION, (char*)variables1[i].name, strlen(variables1[i].name), (char*)variables1[i].var, strlen(variables1[i].var));
          retro->variables.push_back(variables1[i]);
          save = true;
       }
    }
    if (save)
    {
       int size = ini_save(ini, NULL, 0); // Find the size needed
       char* data = (char*)malloc(size);
       size = ini_save(ini, data, size); // Actually save the file
       fp = _wfopen(retro->corevar_path, L"w");
       fwrite(data, 1, size, fp);
       fclose(fp);
       free(data);
    }
    ini_destroy(ini);
  }
}

const char* load_coresettings(retro_variable *var)
{
  CLibretro *retro = CLibretro::GetSingleton();
  for (int i = 0; i < retro->variables.size(); i++)
  {
    if (strcmp(retro->variables[i].name, var->key) == 0)
    {
      return retro->variables[i].var;
    }
  }
  return NULL;
}
#pragma optimize( "", on )  



bool core_environment(unsigned cmd, void *data) {
  bool *bval;
  CLibretro * retro = CLibretro::GetSingleton();
  input *input_device = input::GetSingleton();
  switch (cmd) {
  case RETRO_ENVIRONMENT_SET_MESSAGE: {
    struct retro_message *cb = (struct retro_message *)data;
    return true;
  }
  case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
    struct retro_log_callback *cb = (struct retro_log_callback *)data;
    cb->log = core_log;
    return true;
  }
  case RETRO_ENVIRONMENT_GET_CAN_DUPE:
    bval = (bool*)data;
    *bval = true;
    return true;
  case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: // 9
  case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: // 31
  {
    static char *sys_path = NULL;
    if (!sys_path)
    {
      string ansi = utf8_from_utf16(retro->sys_filename);
      sys_path = strdup(ansi.c_str());
    }
    char **ppDir = (char**)data;
    *ppDir = sys_path;
    return true;
  }
  break;

  case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: // 31
  {
    char variable_val2[50] = { 0 };
    Std_File_Reader_u out;
    lstrcpy(input_device->path, retro->inputcfg_path);
    if (!out.open(retro->inputcfg_path))
    {
      const char *err = input_device->load(out);
      if (!err)
      {
        struct retro_input_descriptor *var = (struct retro_input_descriptor *)data;
        static int i = 0;
        while (var != NULL && var->port == 0) {
          var++; i++;
        }
        if (i != input_device->bl->get_count())
        {
          out.close();
          input_device->bl->clear();
          goto init;
        }

      }
      out.close();
    }
    else
    {
    init:
      struct retro_input_descriptor *var = (struct retro_input_descriptor *)data;
      static int i = 0;
      while (var != NULL && var->port == 0)
      {
        dinput::di_event keyboard;
        keyboard.type = dinput::di_event::ev_none;
        keyboard.key.type = dinput::di_event::key_none;
        keyboard.key.which = NULL;
        CString str = var->description;
        int id = var->id;
        int index = var->index;
        if (var->device == RETRO_DEVICE_ANALOG || (var->device == RETRO_DEVICE_JOYPAD))
        {
          if (var->device == RETRO_DEVICE_ANALOG)
            id = (index == RETRO_DEVICE_INDEX_ANALOG_LEFT) ? (var->id == RETRO_DEVICE_ID_ANALOG_X ? 16 : 17) :
            (var->id == RETRO_DEVICE_ID_ANALOG_X ? 18 : 19);

          input_device->bl->add(keyboard, i, str.GetBuffer(NULL), id);
        }
        i++; ++var;
      }
      Std_File_Writer_u out2;
      if (!out2.open(retro->inputcfg_path))
      {
        input_device->save(out2);
        out2.close();
      }
    }
    return true;
  }
  break;

  case RETRO_ENVIRONMENT_SET_VARIABLES:
  {
    struct retro_variable *var = (struct retro_variable *)data;
    init_coresettings(var);
    return true;
  }
  break;

  case RETRO_ENVIRONMENT_GET_VARIABLE:
  {
    struct retro_variable * variable = (struct retro_variable*)data;
    variable->value = load_coresettings(variable);
    return true;
  }
  break;

  case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
  {
    *(bool*)data = retro->variables_changed;
    retro->variables_changed = false;
    return true;
  }
  break;

  case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
    const enum retro_pixel_format *fmt = (enum retro_pixel_format *)data;
    if (*fmt > RETRO_PIXEL_FORMAT_RGB565)
      return false;
    return video_set_pixel_format(*fmt);
  }
  case RETRO_ENVIRONMENT_SET_HW_RENDER: {
    struct retro_hw_render_callback *hw = (struct retro_hw_render_callback*)data;
    if (hw->context_type == RETRO_HW_CONTEXT_VULKAN)return false;
    hw->get_current_framebuffer = core_get_current_framebuffer;
    hw->get_proc_address = (retro_hw_get_proc_address_t)get_proc;
    g_video.hw = *hw;
    return true;
  }
  default:
    core_log(RETRO_LOG_DEBUG, "Unhandled env #%u", cmd);
    return false;
  }

  return false;
}

static void core_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch) {
  video_refresh(data, width, height, pitch);
}


static void core_input_poll(void) {
  input *input_device = input::GetSingleton();
  input_device->poll();
}

static int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
  if (port != 0)return 0;
  input *input_device = input::GetSingleton();
  if (input_device && input_device->bl != NULL)
  {
    for (unsigned int i = 0; i < input_device->bl->get_count(); i++) {
      {
        if (device == RETRO_DEVICE_ANALOG || device == RETRO_DEVICE_JOYPAD)
        {
          int retro_id = 0;
          int16_t value = 0;
          bool isanalog = false;
          input_device->getbutton(i, value, retro_id, isanalog);
          if (device == RETRO_DEVICE_ANALOG)
          {
            if (value <= -0x8000)value = -0x7fff;
            if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT)
            {
              if ((id == RETRO_DEVICE_ID_ANALOG_X && retro_id == 16) || (id == RETRO_DEVICE_ID_ANALOG_Y && retro_id == 17))
              {
                return isanalog ? -value : value;
              }
            }
            else
            {
              if ((id == RETRO_DEVICE_ID_ANALOG_X && retro_id == 18) || (id == RETRO_DEVICE_ID_ANALOG_Y && retro_id == 19))
              {
                return isanalog ? -value : value;
              }
            }
          }
          else
          {
            value = abs(value);
            if (retro_id == id)return value;
          }
        }
      }
    }
  }
  return 0;
}

void CLibretro::core_audio_sample(int16_t left, int16_t right)
{
  int16_t buf[2] = { left, right };
  _audio.mix(buf, 1);
}

size_t CLibretro::core_audio_sample_batch(const int16_t *data, size_t frames)
{
  _audio.mix(data, frames);
  return frames;
}

bool CLibretro::savestate(TCHAR* filename, bool save)
{
  if (isEmulating)
  {
    size_t size = g_retro.retro_serialize_size();
    if (size)
    {
      if (save)
      {
        FILE *Input = _wfopen(filename, L"wb");
        if (!Input)return false;
        // Get the filesize
        BYTE *Memory = (BYTE *)malloc(size);
        g_retro.retro_serialize(Memory, size);
        if (Memory)fwrite(Memory, 1, size, Input);
        fclose(Input);
        Input = NULL;
        return true;

      }
      else
      {
        FILE *Input = _wfopen(filename, L"rb");
        if (!Input)return false;
        fseek(Input, 0, SEEK_END);
        int Size = ftell(Input);
        fseek(Input, 0, SEEK_SET);
        BYTE *Memory = (BYTE *)malloc(Size);
        if (!Memory)
        {
          fclose(Input);
          return false;
        }
        fread(Memory, 1, Size, Input);
        g_retro.retro_unserialize(Memory, size);
        fclose(Input);
        Input = NULL;
        return true;
      }
    }
  }
  return false;
}

bool CLibretro::savesram(TCHAR* filename, bool save)
{
  if (isEmulating)
  {
    size_t size = g_retro.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    if (size)
    {
      if (save)
      {
        FILE *Input = _wfopen(filename, L"wb");
        if (!Input) return(NULL);
        // Get the filesize
        BYTE *Memory = (BYTE *)g_retro.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
        fwrite(Memory, 1, size, Input);
        fclose(Input);
        Input = NULL;
        return true;

      }
      else
      {
        FILE *Input = _wfopen(filename, L"rb");
        if (!Input) return(NULL);
        fseek(Input, 0, SEEK_END);
        int Size = ftell(Input);
        fseek(Input, 0, SEEK_SET);
        BYTE *Memory_load = (BYTE *)g_retro.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
        fread(Memory_load, 1, Size, Input);
        fclose(Input);
        Input = NULL;
        return true;
      }
    }
  }
  return false;
}

void CLibretro::reset()
{
  if (isEmulating)g_retro.retro_reset();
}


static void core_audio_sample(int16_t left, int16_t right) {
  CLibretro* lib = CLibretro::GetSingleton();
  if (lib->isEmulating)
    lib->core_audio_sample(left, right);
}

static size_t core_audio_sample_batch(const int16_t *data, size_t frames) {
  CLibretro* lib = CLibretro::GetSingleton();
  if (lib->isEmulating)
  {
    lib->core_audio_sample_batch(data, frames);
   return frames;
  }
  else return 0;
}

bool CLibretro::core_load(TCHAR *sofile, bool gamespecificoptions, TCHAR* game_filename) {

  memset(&g_retro, 0, sizeof(g_retro));
  g_retro.handle = LoadLibrary(sofile);
  if (!g_retro.handle)return false;

#define die() do { FreeLibrary(g_retro.handle); return false; } while(0)
#define libload(name) GetProcAddress(g_retro.handle, name)
#define load(name) if (!(*(void**)(&g_retro.#name)=(void*)libload(#name))) die()
#define load_sym(V,name) if (!(*(void**)(&V)=(void*)libload(#name))) die()
#define load_retro_sym(S) load_sym(g_retro.S, S)
  load_retro_sym(retro_init);
  load_retro_sym(retro_deinit);
  load_retro_sym(retro_api_version);
  load_retro_sym(retro_get_system_info);
  load_retro_sym(retro_get_system_av_info);
  load_retro_sym(retro_set_controller_port_device);
  load_retro_sym(retro_reset);
  load_retro_sym(retro_run);
  load_retro_sym(retro_load_game);
  load_retro_sym(retro_unload_game);
  load_retro_sym(retro_serialize);
  load_retro_sym(retro_unserialize);
  load_retro_sym(retro_serialize_size);
  load_retro_sym(retro_get_memory_size);
  load_retro_sym(retro_get_memory_data);
  void(*set_environment)(retro_environment_t) = NULL;
  void(*set_video_refresh)(retro_video_refresh_t) = NULL;
  void(*set_input_poll)(retro_input_poll_t) = NULL;
  void(*set_input_state)(retro_input_state_t) = NULL;
  void(*set_audio_sample)(retro_audio_sample_t) = NULL;
  void(*set_audio_sample_batch)(retro_audio_sample_batch_t) = NULL;
#define load_sym(V,name) if (!(*(void**)(&V)=(void*)libload(#name)))
  load_sym(set_environment, retro_set_environment);
  load_sym(set_video_refresh, retro_set_video_refresh);
  load_sym(set_input_poll, retro_set_input_poll);
  load_sym(set_input_state, retro_set_input_state);
  load_sym(set_audio_sample, retro_set_audio_sample);
  load_sym(set_audio_sample_batch, retro_set_audio_sample_batch);

  TCHAR filez[MAX_PATH] = { 0 };
  lstrcpy(filez, game_filename);
  PathStripPath(filez);
  PathRemoveExtension(filez);
  TCHAR core_handlepath[MAX_PATH] = { 0 };
  GetModuleFileNameW(g_retro.handle, core_handlepath, sizeof(core_handlepath));
  PathRemoveExtension(core_handlepath);
  GetCurrentDirectory(MAX_PATH, sys_filename);
  PathAppend(sys_filename, L"system");
  lstrcpy(sav_filename, sys_filename);
  PathAppend(sav_filename, filez);
  lstrcat(sav_filename, L".sav");

  lstrcpy(inputcfg_path, L"");
  lstrcpy(corevar_path, L"");

  if (gamespecificoptions)
  {
    lstrcpy(inputcfg_path, core_handlepath);
    lstrcpy(corevar_path, core_handlepath);
    PathAppend(inputcfg_path, filez);
    PathAppend(corevar_path, filez);
    lstrcat(inputcfg_path, L"_input.cfg");
    lstrcat(corevar_path, L".ini");
  }
  else
  {
    lstrcat(inputcfg_path, core_handlepath);
    lstrcat(corevar_path, core_handlepath);
    lstrcat(inputcfg_path, L"_input.cfg");
    lstrcat(corevar_path, L".ini");
  }
  //set libretro func pointers
  set_environment(core_environment);
  set_video_refresh(core_video_refresh);
  set_input_poll(core_input_poll);
  set_input_state(core_input_state);
  set_audio_sample(::core_audio_sample);
  set_audio_sample_batch(::core_audio_sample_batch);
  g_retro.retro_init();
  g_retro.initialized = true;
  return true;
}

static void noop() {
}

CLibretro* CLibretro::m_Instance = 0;
CLibretro* CLibretro::CreateInstance(HWND hwnd)
{
  if (0 == m_Instance)
  {
    m_Instance = new CLibretro();
    m_Instance->init(hwnd);
  }
  return m_Instance;
}

CLibretro* CLibretro::GetSingleton()
{
  return m_Instance;
}
//////////////////////////////////////////////////////////////////////////////////////////

bool CLibretro::running()
{
  return isEmulating;
}

CLibretro::CLibretro()
{
  isEmulating = false;
}

static DWORD WINAPI libretro_thread(void* Param)
{
  CLibretro* This = (CLibretro*)Param;
  return This->ThreadStart();
}

DWORD CLibretro::ThreadStart(void)
{
  init_common();
  // Do stuff
  while (isEmulating)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    g_retro.retro_run();
    double currentTime = double(milliseconds_now() / 1000);
    nbFrames++;
    if (currentTime - lastTime >= 0.5) { // If last prinf() was more than 1 sec ago
                       // printf and reset timer
      TCHAR buffer[100] = { 0 };
      int len = swprintf(buffer, 100, L"einwegger�t: %2f ms/frame\n, %d FPS", 1000.0 / double(nbFrames), nbFrames);
      SetWindowText(emulator_hwnd, buffer);
      nbFrames = 0;
      lastTime += 1.0;
    }
  }
  _audio.destroy();
  video_deinit();
  g_retro.retro_unload_game();
  if (info.data)
    free((void*)info.data);
  g_retro.retro_deinit();
  return 0;
}

CLibretro::~CLibretro(void)
{
  if (isEmulating)isEmulating = false;
  kill();
}

bool CLibretro::init_common()
{
  variables.clear();
  struct retro_system_info system = { 0 };
  g_video = { 0 };
  g_video.hw.version_major = 3;
  g_video.hw.version_minor = 3;
  g_video.hw.context_type = RETRO_HW_CONTEXT_NONE;
  g_video.hw.context_reset = NULL;
  g_video.hw.context_destroy = NULL;
  variables_changed = false;
  if (!core_load(core_path, gamespec, rom_path))
  {
    printf("FAILED TO LOAD CORE!!!!!!!!!!!!!!!!!!");
    return false;
  }
  CHAR szFileName[MAX_PATH] = { 0 };
  string ansi = utf8_from_utf16(rom_path);
  strcpy(szFileName, ansi.c_str());
  struct stat st;
  stat(szFileName, &st);
  info.path = szFileName;
  info.data = NULL;
  info.size = st.st_size;
  info.meta = "";
  g_retro.retro_get_system_info(&system);
  if (!system.need_fullpath) {
    FILE *inputfile = _wfopen(rom_path, L"rb");
    if (!inputfile)
    {
    fail:
      printf("FAILED TO LOAD ROMz!!!!!!!!!!!!!!!!!!");
      return false;
    }
    info.data = malloc(info.size);
    if (!info.data)goto fail;
    size_t read = fread((void*)info.data, 1, info.size, inputfile);
    fclose(inputfile);
    inputfile = NULL;
  }
  if (!g_retro.retro_load_game(&info))
  {
    printf("FAILED TO LOAD ROM!!!!!!!!!!!!!!!!!!");
    return false;
  }
  retro_system_av_info av = { 0 };
  g_retro.retro_get_system_av_info(&av);
  ::video_configure(&av.geometry, emulator_hwnd);
  double refreshr = 0;
  DWM_TIMING_INFO timing_info = { 0 };
  timing_info.cbSize = sizeof(timing_info);
  DwmGetCompositionTimingInfo(NULL, &timing_info);
  refreshr = timing_info.rateRefresh.uiNumerator;
  _audio.init(refreshr, av);
  lastTime = (double)milliseconds_now() / 1000;
  nbFrames = 0;
  isEmulating = true;
  return true;
}

bool CLibretro::loadfile(TCHAR* filename, TCHAR* core_filename, bool gamespecificoptions, bool mthreaded)
{
  if (isEmulating)isEmulating = false;
  gamespec = gamespecificoptions;
  lstrcpy(rom_path, filename);
  lstrcpy(core_path, core_filename);
  threaded = mthreaded;
//  logfile = fopen("file.txt", "w");
  if (threaded)
  {
    thread_handle = CreateThread(NULL, 0, &libretro_thread, (void*)this, 0, &thread_id);
    return true;
  }
  else
  {
    bool common = init_common();
    return common;
  }
}

void CLibretro::run()
{
if (!threaded)
  {
     glBindFramebuffer(GL_FRAMEBUFFER, 0);
     glClearColor(0, 0, 0, 1);
     glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
     g_retro.retro_run();
     
     double currentTime = (double)milliseconds_now() / 1000;
    

    if (currentTime - lastTime >= 0.5) { // If last prinf() was more than 1 sec ago
                       // printf and reset timer
      TCHAR buffer[200] = { 0 };
      int len = swprintf(buffer, 200, L"einwegger�t: %2f ms/frame\n, min %d VPS", 1000.0 / double(nbFrames), nbFrames);
      SetWindowText(emulator_hwnd, buffer);
      nbFrames = 0;
      lastTime += 1.0;
    }
    nbFrames++;

  }
}

bool CLibretro::init(HWND hwnd)
{
  isEmulating = false;
  emulator_hwnd = hwnd;
  return true;
}

void CLibretro::kill()
{
  isEmulating = false;
  if (threaded)
  {
    WaitForSingleObject(thread_handle, INFINITE);
    CloseHandle(thread_handle);
  }
  else
  {
  
    g_retro.retro_unload_game();
    g_retro.retro_deinit();
    if (info.data)
      free((void*)info.data);
    _audio.destroy();
  //  fclose(logfile);
    video_deinit();
  }
}