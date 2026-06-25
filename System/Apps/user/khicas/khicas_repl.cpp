/**
 * @file System/Apps/user/khicas/khicas_repl.cpp
 * @brief Corrected KhiCAS console REPL entry point.
 *
 * The prebuilt libkcasgui.libcpp ships kcas_main() (External/KhiCAS/khicas/
 * kdisplay.cc, #ifdef HP39). Its REPL loop mistakes the "user left the console"
 * signal for a fault: Console_GetLine() returns NULL when the user presses
 * Home/MENU (KEY_CTRL_MENU -- see Console_GetLine in kdisplay.cc), i.e. picks
 * "Quit" from the Home menu. kcas_main treats that NULL as confirm("memory
 * error") and then spins forever in a dead for(;;)GetKey loop. The user sees:
 * Home (opens menu) -> Home again -> "Memory Error" and a hang.
 *
 * giac::console_main() (the canonical loop in the same file) handles the very
 * same NULL correctly, as a clean exit. We cannot patch the prebuilt blob from
 * our CMake build, so this file re-implements kcas_main's setup verbatim but
 * exits cleanly on NULL (save session -> free console -> release globals ->
 * return). test.cpp calls khicas_repl() instead of kcas_main().
 *
 * This translation unit pulls in the giac headers directly, so it must be built
 * with -DKHICAS -DHAVE_CONFIG_H (and libtommath on the include path), matching
 * the flags the prebuilt giac libraries were compiled with, so giac type
 * layouts (context, gen, ...) stay ABI-compatible. See the CMakeLists in this
 * directory.
 */

#include "config.h"
#include "giacPCH.h"
#include "kdisplay.h"
#include <string.h>

using namespace std;
using namespace giac;

// Defined in the prebuilt blob (kdisplay.cc, #ifdef HP39): the GUI's current
// giac context pointer and the shutdown auto-save handler. We reuse the same
// global so the registered SetQuitHandler still saves the right context.
extern giac::context *contextptr;
void quit_save_session();
extern "C" void SetQuitHandler(void (*callback)(void)); // syscalls.h

// restore_session() is defined in xcas namespace in the prebuilt blob (kdisplay.cc)
// but, unlike save_session/Console_*, it is NOT declared in kdisplay.h. Declare it
// here so this TU can call it the same way kcas_main does.
namespace xcas {
  int restore_session(const char *fname, const giac::context *);
}

int khicas_repl(int isAppli, unsigned short OptionNum)
{
  (void)isAppli;
  (void)OptionNum;

  size_t rambase = 0x02000000 + 4096; // 4096 for the 1bpp screen buffer
  tab16 = (four_int *)rambase;
  tab24 = (six_int *)((size_t)tab16 + 4096);
  tab48 = (twelve_int *)((size_t)tab24 + 16 * 32 * 24);

  char *expr;

  SetQuitHandler(quit_save_session); // automatically save session when exiting

  turtle();
#ifdef TURTLETAB
  turtle_stack_size = 0;
#else
  turtle_stack(); // required to init turtle
#endif

  context ct;
  contextptr = &ct;
  xcas::Console_Init(contextptr);
  giac::_srand(vecteur(0), contextptr);
  xcas::restore_session("session", contextptr);
  xcas::Console_Disp(1, contextptr);
  lang = 0;

  while (1)
  {
    if ((expr = xcas::Console_GetLine(contextptr)) == NULL)
    {
      // NULL means the user left the console (Home/MENU -> "Quit"), NOT a
      // memory fault. Exit cleanly back to the launcher, mirroring the cleanup
      // done by giac::console_main()'s NULL branch.
      xcas::save_session(contextptr);
      xcas::Console_Free();
      giac::release_globals();
      return 0;
    }
    if (strcmp((const char *)expr, "restart") == 0)
    {
      if (confirm(lang ? "Effacer variables?" : "Clear variables?",
                  lang ? "F1: annul,  F6: confirmer" : "F1: cancel,  F6: confirm") != KEY_CTRL_F6)
      {
        xcas::Console_Output((const char *)" cancelled");
        xcas::Console_NewLine(xcas::LINE_TYPE_OUTPUT, 1);
        xcas::Console_Disp(1, contextptr);
        continue;
      }
    }
    if (strcmp((const char *)expr, "=>") == 0 || strcmp((const char *)expr, "=>\n") == 0)
    {
      xcas::save_session(contextptr);
      xcas::Console_Output("Session saved");
    }
    else
      xcas::run((char *)expr, 7, contextptr);
    xcas::Console_NewLine(xcas::LINE_TYPE_OUTPUT, 1);
    xcas::Console_Disp(1, contextptr);
  }
}
