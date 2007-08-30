#
# Copyright (C) 2007, Jonathan S. Shapiro.
#
# This file is part of the Coyotos Operating System.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2,
# or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#

# program to generate wrapper headers to allow common inclusion of
# machine-dependent header files.

BEGIN {
  for (i = 1; i < ARGC; i++)
    fn[i-1]=ARGV[i];

  nFile = ARGC-1;
  ARGC=1;
  nArch = 0;
}

/^[ \t]*\#.*/   { next; }

/^[ \t]*$/   { next; }

NF != 0 {
  arch[nArch] = $1;
  test[nArch] = $2;
  nArch = nArch + 1;
}

END {
  for (i = 0; i < nFile; i++) {
    print "Processing " fn[i];
    printf("#ifndef __COYOTOS_MACHINE_%s__\n", nmgen(fn[i])) > fn[i];
    printf("#define __COYOTOS_MACHINE_%s__\n", nmgen(fn[i])) > fn[i];
    printf("/** @file\n") >> fn[i];
    printf(" * @brief Automatically generated architecture wrapper header.\n") >> fn[i];
    printf(" */\n") >> fn[i];

    printf("\n") >> fn[i];
    printf("#include \"target.h\"\n") >> fn[i];
    printf("\n") >> fn[i];

    for (a = 0; a < nArch; a++) {
      printf("#if (COYOTOS_TARGET == COYOTOS_TARGET_%s) || defined(COYOTOS_UNIVERSAL_CROSS)\n", arch[a]) > fn[i];

      printf("#include \"../%s/%s\"\n", arch[a], fn[i]) > fn[i];

      printf("#endif\n\n") > fn[i];
    }

    printf("#endif /* __COYOTOS_MACHINE_%s__ */\n", nmgen(fn[i])) > fn[i];
  }
}

function nmgen(nm)
{
  nm = toupper(nm);
  gsub(/[.-]/, "_", nm);

  return nm;
}
