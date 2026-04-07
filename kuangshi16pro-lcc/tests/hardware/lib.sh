#!/usr/bin/env bash

lcc_hw_repo_root() {
  (
    cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd
  )
}

lcc_hw_project_dir() {
  local repo_root

  repo_root="$(lcc_hw_repo_root)"
  printf '%s/kuangshi16pro-lcc\n' "$repo_root"
}

lcc_hw_now_stamp() {
  date '+%Y%m%d-%H%M%S'
}

lcc_hw_log() {
  printf '[hardware-smoke] %s\n' "$*"
}

lcc_hw_is_truthy() {
  case "${1:-}" in
    1|true|yes|on)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

lcc_hw_require_command() {
  local command_name="$1"

  if ! command -v "$command_name" >/dev/null 2>&1; then
    printf 'required command not found: %s\n' "$command_name" >&2
    return 1
  fi
}

lcc_hw_require_file() {
  local path="$1"

  if [[ ! -f "$path" ]]; then
    printf 'required file not found: %s\n' "$path" >&2
    return 1
  fi
}

lcc_hw_json_get() {
  local file="$1"
  local key="$2"
  local match=""
  local value=""

  if [[ ! -f "$file" ]]; then
    printf 'none\n'
    return 0
  fi

  match="$(grep -o "\"$key\":\\(\"[^\"]*\"\\|null\\|true\\|false\\|[0-9][0-9]*\\)" \
    "$file" | head -n 1 || true)"
  if [[ -z "$match" ]]; then
    printf 'none\n'
    return 0
  fi

  value="${match#*:}"
  value="${value#\"}"
  value="${value%\"}"
  if [[ -z "$value" || "$value" == "null" ]]; then
    value="none"
  fi

  printf '%s\n' "$value"
}

lcc_hw_write_summary() {
  local state_file="$1"
  local summary_file="$2"
  local command_rc="$3"
  local state_rc="$4"

  {
    printf 'command_exit_code=%s\n' "$command_rc"
    printf 'state_capture=%s\n' "$([[ "$state_rc" -eq 0 ]] && printf 'ok' || printf 'failed')"
    printf 'backend=%s\n' "$(lcc_hw_json_get "$state_file" "backend")"
    printf 'backend_selected=%s\n' \
      "$(lcc_hw_json_get "$state_file" "backend_selected")"
    printf 'backend_fallback_reason=%s\n' \
      "$(lcc_hw_json_get "$state_file" "backend_fallback_reason")"
    printf 'last_apply_stage=%s\n' \
      "$(lcc_hw_json_get "$state_file" "last_apply_stage")"
    printf 'last_apply_error=%s\n' \
      "$(lcc_hw_json_get "$state_file" "last_apply_error")"
    printf 'last_apply_backend=%s\n' \
      "$(lcc_hw_json_get "$state_file" "last_apply_backend")"
  } >"$summary_file"
}

lcc_hw_print_summary() {
  local summary_file="$1"

  if [[ -f "$summary_file" ]]; then
    sed 's/^/[hardware-smoke]   /' "$summary_file"
  fi
}

lcc_hw_journal_cursor() {
  local service_name="$1"

  sudo journalctl -u "$service_name" -n 0 --show-cursor --no-pager 2>/dev/null |
    sed -n 's/^-- cursor: //p' | tail -n 1
}

lcc_hw_capture_journal() {
  local service_name="$1"
  local cursor="$2"
  local lines="$3"
  local output_file="$4"

  if [[ -n "$cursor" ]]; then
    sudo journalctl -u "$service_name" --after-cursor "$cursor" \
      -n "$lines" --no-pager -o short-iso >"$output_file"
    return 0
  fi

  sudo journalctl -u "$service_name" -n "$lines" --no-pager -o short-iso \
    >"$output_file"
}

lcc_hw_write_matrix_override() {
  local matrix="$1"
  local output_file="$2"

  case "$matrix" in
    standard-only)
      cat >"$output_file" <<EOF
[Service]
UnsetEnvironment=LCC_AMW0_DRY_RUN
Environment=LCC_BACKEND=standard
EOF
      ;;
    amw0-forced)
      cat >"$output_file" <<EOF
[Service]
UnsetEnvironment=LCC_AMW0_DRY_RUN
Environment=LCC_BACKEND=amw0
EOF
      ;;
    mixed)
      cat >"$output_file" <<EOF
[Service]
UnsetEnvironment=LCC_BACKEND LCC_AMW0_DRY_RUN
EOF
      ;;
    dry-run)
      cat >"$output_file" <<EOF
[Service]
UnsetEnvironment=LCC_BACKEND
Environment=LCC_AMW0_DRY_RUN=1
EOF
      ;;
    *)
      printf 'unsupported matrix: %s\n' "$matrix" >&2
      return 1
      ;;
  esac
}
