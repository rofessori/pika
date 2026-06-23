#!/usr/bin/env bash
#
# pika — installer.
#
# A plain, portable, interactive installer. It prints clear prompts and reads
# your answers; there is no curses/dialog dependency, so it behaves identically
# on Linux and on macOS (including the old bash that ships with macOS).
#
# Non-interactive use (no prompts):
#   ./setup.sh --essentials                 # library + CLI only (recommended)
#   ./setup.sh --reader                     # essentials + terminal reader
#   ./setup.sh --everything                 # essentials + reader + web app
#   ./setup.sh --everything --prefix ~/.local --yes
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# We deliberately avoid `set -u`: it aborts on references it deems unbound,
# which is fragile across shells. We keep `set -e` and pipefail instead.
set -eo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${PREFIX:-/usr/local}"
PROFILE=""          # essentials | reader | everything | (empty => ask)
ASSUME_YES=0
WITH_TUI=0
WITH_WEB=0

# ---- colours / output ----------------------------------------------------
if [ -t 1 ]; then
  bold=$(tput bold 2>/dev/null || true); dim=$(tput dim 2>/dev/null || true)
  red=$(tput setaf 1 2>/dev/null || true); grn=$(tput setaf 2 2>/dev/null || true)
  rst=$(tput sgr0 2>/dev/null || true)
else
  bold=""; dim=""; red=""; grn=""; rst=""
fi
say()  { printf '%s\n' "$*"; }
ok()   { printf '  %s✓%s %s\n' "$grn" "$rst" "$*"; }
warn() { printf '  %s!%s %s\n' "$red" "$rst" "$*"; }
die()  { printf '%serror:%s %s\n' "$red" "$rst" "$*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

# Interactive only when both ends are a terminal and --yes was not given.
INTERACTIVE=0
[ -t 0 ] && [ -t 1 ] && [ "$ASSUME_YES" -eq 0 ] && INTERACTIVE=1

usage() {
  cat <<EOF
pika installer

Usage: ./setup.sh [options]

  --essentials     install the library + CLI only (recommended default)
  --reader         also install the terminal reader (pika-tui)
  --everything     also build the optional web app
  --prefix DIR     install prefix (default: $PREFIX)
  --yes            do not prompt; accept defaults (non-interactive)
  --help           show this help
EOF
}

# ---- argument parsing ----------------------------------------------------
while [ $# -gt 0 ]; do
  case "$1" in
    --essentials) PROFILE=essentials ;;
    --reader)     PROFILE=reader ;;
    --everything) PROFILE=everything ;;
    --prefix)     PREFIX="${2:?--prefix needs a directory}"; shift ;;
    --prefix=*)   PREFIX="${1#*=}" ;;
    --yes|-y)     ASSUME_YES=1; INTERACTIVE=0 ;;
    --help|-h)    usage; exit 0 ;;
    *) die "unknown option: $1 (try --help)" ;;
  esac
  shift
done

# ---- screens -------------------------------------------------------------
hr() { say "${dim}------------------------------------------------------------${rst}"; }

banner() {
  say ""
  say "  ${bold}p${rst}${red}${bold}i${rst}${bold}ka${rst} — fast focal-point reading"
  say "  ${dim}read text one word at a time, on a fixed point, at your pace${rst}"
  say ""
  say "  This installs the pika C library and the ${bold}pika${rst} command, and can"
  say "  optionally add the terminal reader and the web app."
}

check_prereqs() {
  local missing=""
  have cc || have gcc || missing="${missing}  - a C compiler (gcc or clang)\n"
  have make || missing="${missing}  - make\n"
  if [ -n "$missing" ]; then
    say ""
    warn "pika needs the following to build:"
    printf '%b' "$missing"
    say ""
    say "  macOS:        xcode-select --install"
    say "  Debian/Ubuntu: sudo apt install build-essential"
    die "missing build prerequisites"
  fi
}

capabilities() {
  say ""
  say "  ${bold}Optional document features${rst} (the library works without them):"
  have pdftotext && ok "PDF reading    (pdftotext found)" \
                 || say "  ${dim}-${rst} PDF reading    (install poppler-utils to enable)"
  have unzip && ok "DOCX reading   (unzip found)" \
             || say "  ${dim}-${rst} DOCX reading   (install unzip to enable)"
  { have curl || have wget; } && ok "URL fetching   (curl/wget found)" \
             || say "  ${dim}-${rst} URL fetching   (install curl or wget to enable)"
  have node && ok "Web app        (Node.js found)" \
            || say "  ${dim}-${rst} Web app        (install Node.js 18+ to enable)"
}

# Prompts are written to stderr so the chosen value (on stdout) survives
# command substitution.
ask_profile() {
  if [ "$INTERACTIVE" -ne 1 ]; then printf 'essentials'; return; fi
  {
    say ""
    say "${bold}What would you like to install?${rst}"
    say "  ${bold}1${rst}) Essentials  - the pika library and CLI            ${dim}(recommended)${rst}"
    say "  ${bold}2${rst}) Reader      - also the terminal reader (pika-tui)"
    say "  ${bold}3${rst}) Everything  - also the web app (needs Node.js)"
    printf "Enter 1, 2 or 3 [1]: "
  } >&2
  local choice; read -r choice
  case "${choice:-1}" in
    2) printf 'reader' ;;
    3) printf 'everything' ;;
    *) printf 'essentials' ;;
  esac
}

ask_prefix() {
  if [ "$INTERACTIVE" -ne 1 ]; then printf '%s' "$PREFIX"; return; fi
  {
    say ""
    say "${bold}Where should pika be installed?${rst}"
    say "  ${dim}/usr/local${rst}     system-wide (asks for your password via sudo)"
    say "  ${dim}\$HOME/.local${rst}   just for you (no password needed)"
    printf "Install prefix [%s]: " "$PREFIX"
  } >&2
  local p; read -r p
  printf '%s' "${p:-$PREFIX}"
}

confirm() {
  [ "$INTERACTIVE" -eq 1 ] || return 0
  printf "\n%sProceed with the installation? [Y/n]:%s " "$bold" "$rst" >&2
  local a; read -r a
  case "${a:-y}" in [nN]*) return 1 ;; esac
  return 0
}

apply_profile() {
  case "$PROFILE" in
    essentials) WITH_TUI=0; WITH_WEB=0 ;;
    reader)     WITH_TUI=1; WITH_WEB=0 ;;
    everything) WITH_TUI=1; WITH_WEB=1 ;;
    *)          WITH_TUI=0; WITH_WEB=0; PROFILE=essentials ;;
  esac
}

# ---- install steps -------------------------------------------------------
# True if installing to $PREFIX will require sudo.
needs_sudo() {
  local target="$PREFIX"
  while [ ! -d "$target" ]; do target="$(dirname "$target")"; done
  [ ! -w "$target" ]
}

maybe_sudo() {
  if ! needs_sudo; then "$@"; return; fi
  have sudo || die "cannot write to $PREFIX and sudo is unavailable; re-run with --prefix \$HOME/.local"
  sudo "$@"
}

build_core() {
  say ""; say "${bold}Building the library and CLI...${rst}"
  make -C "$ROOT/core" lib cli >/dev/null
  [ "$WITH_TUI" -eq 1 ] && make -C "$ROOT/core" tui >/dev/null
  make -C "$ROOT/core" check >/dev/null && ok "unit tests passed"
  ok "build complete"
}

install_core() {
  say ""; say "${bold}Installing to $PREFIX...${rst}"
  if needs_sudo; then
    warn "this location needs administrator rights — you may be asked for your password"
  fi
  maybe_sudo make -C "$ROOT/core" install PREFIX="$PREFIX" >/dev/null
  ok "installed libpika, headers, and the pika CLI"
  if [ "$WITH_TUI" -eq 1 ]; then
    maybe_sudo make -C "$ROOT/core" install-tui PREFIX="$PREFIX" >/dev/null
    ok "installed the pika-tui reader"
  fi
  have ldconfig && [ -w "$PREFIX/lib" ] && ldconfig 2>/dev/null || true
}

setup_web() {
  say ""; say "${bold}Setting up the web app...${rst}"
  have node || die "Node.js 18+ is required for the web app (https://nodejs.org)"
  ( cd "$ROOT/server" && npm install --omit=dev --no-audit --no-fund >/dev/null )
  ok "installed server dependencies"
  ( cd "$ROOT/web" && npm install --no-audit --no-fund >/dev/null && npm run build >/dev/null )
  ok "built the web frontend"
  [ -f "$ROOT/server/.env" ] || { cp "$ROOT/server/.env.example" "$ROOT/server/.env"; ok "created server/.env"; }
}

done_screen() {
  say ""; hr
  ok "${bold}pika is installed.${rst}"
  say ""
  say "  Try it:"
  say "    ${dim}echo 'Reading on a fixed point is fast.' | pika --json${rst}"
  [ "$WITH_TUI" -eq 1 ] && say "    ${dim}pika-tui /path/to/file.txt${rst}"
  if [ "$WITH_WEB" -eq 1 ]; then
    say ""
    say "  Start the web app:"
    say "    ${dim}cd server && PIKA_BIN=$PREFIX/bin/pika npm start${rst}"
    say "  ${dim}The admin console URL + one-time passkey are printed on first start${rst}"
    say "  ${dim}and saved to server/data/admin-access.txt.${rst}"
  fi
  case ":$PATH:" in
    *":$PREFIX/bin:"*) : ;;
    *) say ""; warn "add ${bold}$PREFIX/bin${rst} to your PATH to run 'pika' directly" ;;
  esac
  hr
}

# ---- main ----------------------------------------------------------------
main() {
  banner
  check_prereqs
  capabilities
  [ -n "$PROFILE" ] || PROFILE="$(ask_profile)"
  apply_profile
  [ "$INTERACTIVE" -eq 1 ] && PREFIX="$(ask_prefix)"

  say ""; hr
  say "  Profile        : ${bold}$PROFILE${rst}"
  say "  Install prefix : ${bold}$PREFIX${rst}"
  say "  Terminal reader: $([ $WITH_TUI -eq 1 ] && echo yes || echo no)"
  say "  Web app        : $([ $WITH_WEB -eq 1 ] && echo yes || echo no)"
  hr

  if ! confirm; then say "Cancelled."; exit 0; fi

  build_core
  install_core
  [ "$WITH_WEB" -eq 1 ] && setup_web
  done_screen
}

main
