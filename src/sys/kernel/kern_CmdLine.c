/*
 * Copyright (C) 2007, The EROS Group, LLC.
 *
 * This file is part of the Coyotos Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/** @file
 * @brief Kernel command line processing.
 */

#include <stddef.h>
#include <kerninc/CommandLine.h>
#include <kerninc/string.h>
#include <kerninc/printf.h>
#include <kerninc/Cache.h>

char CommandLine[COMMAND_LINE_LIMIT];

static bool isParsed = false;
static size_t origLen;

/** @brief Perform an in-place hack on the command line, replacing
 * white space with NULs to facilitate later option parsing.
 *
 * Retain the original command line length so that we can re-insert
 * white space for later retrieval of the kernel command line.
 */
static void
cmdline_parse()
{
  if (isParsed)
    return;

  isParsed = true;

  origLen = strlen(CommandLine);

  for (size_t i = 0; i < origLen; i++) {
    switch(CommandLine[i]) {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
      CommandLine[i] = 0;
      break;
    default:
      break;
    }
  }
}


/** @brief Return the argv[0] element of a command line. */
const char *
cmdline_argv0(const char *cmdline)
{
  cmdline_parse();

  for (size_t i = 0; i < origLen; i++) 
    if (CommandLine[i])
      return &CommandLine[i];

  fatal("Command line had no content\n", cmdline);
}

/** @brief Given a command line, find the named option if present, or
 * return NULL.
 */
static const char *
cmdline_find_option(const char *optName)
{
  cmdline_parse();

  size_t len = strlen(optName);

  for (size_t i = 0; i < origLen; i++) {
    if (CommandLine[i] != *optName)
      continue;

    if (memcmp(&CommandLine[i], optName, len) != 0)
      continue;

    // May be terminated by either '=' (has an argument) or NUL.
    if (CommandLine[len+i] && CommandLine[len+i] != '=')
      continue;

    return &CommandLine[i];
  }

  return NULL;
}

bool 
cmdline_has_option(const char *optName)
{
  return cmdline_find_option(optName);
}

/** @brief Return pointer to option value string, if present, else
    return @p default. */
const char *
cmdline_option_arg(const char *optName)
{
  const char *s = cmdline_find_option(optName);

  if (s == NULL)
    return NULL;

  while (*s && *s != '=')
    s++;

  return s;
}

/** @brief return true if option exists, has a value, and value
 *  matches candidate, else false. */
bool
cmdline_option_isstring(const char *optName, const char *value)
{
  const char *s = cmdline_option_arg(optName);
  return (s && strcmp(s, value) == 0);
}

/** @brief return non-zero if option exists and has an integral value,
 *  else 0. */
unsigned long
cmdline_option_uvalue(const char *optName)
{
  const char *s = cmdline_option_arg(optName);
  if (!s) return 0;

  return strtoul(s, 0, 0);
}

/** @brief Process any command line options.
 *
 * Pick off any command line options that we care about and then
 * release the hold on the memory that the command line occupies.
 */
void
cmdline_process_options()
{
  Cache.c_Process.count = cmdline_option_uvalue("nproc");
  Cache.c_GPT.count = cmdline_option_uvalue("ngpt");
  Cache.c_Endpoint.count = cmdline_option_uvalue("nendpt");
  //   Cache.page.count = cmdline_option_uvalue("npage");
  Cache.dep.count = cmdline_option_uvalue("depend");
}

