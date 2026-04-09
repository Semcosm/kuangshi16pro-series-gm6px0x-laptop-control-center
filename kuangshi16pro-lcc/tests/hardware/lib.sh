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

lcc_hw_expectation_file() {
  local matrix="$1"
  local project_dir

  project_dir="$(lcc_hw_project_dir)"
  printf '%s/tests/hardware/expectations/%s.txt\n' "$project_dir" "$matrix"
}

lcc_hw_json_has_key() {
  local file="$1"
  local key="$2"

  if [[ ! -f "$file" ]]; then
    return 1
  fi

  grep -q "\"$key\":" "$file"
}

lcc_hw_json_get_object_key() {
  local file="$1"
  local object="$2"
  local key="$3"
  local match=""
  local value=""

  if [[ ! -f "$file" ]]; then
    printf 'none\n'
    return 0
  fi

  match="$(grep -o "\"$object\":{[^}]*\"$key\":\\(\"[^\"]*\"\\|null\\|true\\|false\\|[0-9][0-9]*\\)" \
    "$file" | head -n 1 || true)"
  if [[ -z "$match" ]]; then
    printf 'missing\n'
    return 0
  fi

  value="${match##*:}"
  value="${value#\"}"
  value="${value%\"}"
  if [[ -z "$value" || "$value" == "null" ]]; then
    value="none"
  fi

  printf '%s\n' "$value"
}

lcc_hw_json_get_effective_meta_key() {
  local file="$1"
  local key="$2"
  local match=""
  local value=""

  if [[ ! -f "$file" ]]; then
    printf 'none\n'
    return 0
  fi

  case "$key" in
    source)
      match="$(grep -o "\"effective_meta\":{\"source\":\\(\"[^\"]*\"\\|null\\|true\\|false\\|[0-9][0-9]*\\)" \
        "$file" | head -n 1 || true)"
      ;;
    freshness)
      match="$(grep -o "\"effective_meta\":{\"source\":\\(\"[^\"]*\"\\|null\\|true\\|false\\|[0-9][0-9]*\\),\"freshness\":\\(\"[^\"]*\"\\|null\\|true\\|false\\|[0-9][0-9]*\\)" \
        "$file" | head -n 1 || true)"
      ;;
    *)
      printf 'missing\n'
      return 0
      ;;
  esac

  if [[ -z "$match" ]]; then
    printf 'missing\n'
    return 0
  fi

  value="${match##*:}"
  value="${value#\"}"
  value="${value%\"}"
  if [[ -z "$value" || "$value" == "null" ]]; then
    value="none"
  fi

  printf '%s\n' "$value"
}

lcc_hw_collect_missing_state_keys() {
  local file="$1"
  local missing=()
  local key=""
  local required_keys=(
    backend
    backend_selected
    backend_fallback_reason
    effective_meta
    last_apply_stage
    last_apply_error
    last_apply_backend
  )

  for key in "${required_keys[@]}"; do
    if ! lcc_hw_json_has_key "$file" "$key"; then
      missing+=("$key")
    fi
  done

  if [[ "${#missing[@]}" -gt 0 ]]; then
    printf '%s\n' "$(IFS=,; printf '%s' "${missing[*]}")"
    return 0
  fi

  printf '\n'
}

lcc_hw_require_state_contract() {
  local file="$1"
  local missing_keys=""

  missing_keys="$(lcc_hw_collect_missing_state_keys "$file")"
  if [[ -n "$missing_keys" ]]; then
    printf 'state contract missing keys: %s\n' "$missing_keys" >&2
    return 1
  fi

  return 0
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
    printf 'missing\n'
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

lcc_hw_summary_get() {
  local summary_file="$1"
  local key="$2"
  local match=""

  if [[ ! -f "$summary_file" ]]; then
    printf 'missing\n'
    return 0
  fi

  match="$(grep "^$key=" "$summary_file" | head -n 1 || true)"
  if [[ -z "$match" ]]; then
    printf 'missing\n'
    return 0
  fi

  printf '%s\n' "${match#*=}"
}

lcc_hw_value_matches() {
  local actual="$1"
  local expected="$2"
  local candidate=""
  local -a expected_values=()

  IFS='|' read -r -a expected_values <<<"$expected"
  for candidate in "${expected_values[@]}"; do
    if [[ "$actual" == "$candidate" ]]; then
      return 0
    fi
  done

  return 1
}

lcc_hw_validate_step_contract() {
  local step_name="$1"
  local summary_file="$2"
  local expectation_file="$3"
  local failed=0
  local line=""
  local field=""
  local expected=""
  local actual=""
  local matched=0

  if [[ ! -f "$expectation_file" ]]; then
    printf 'expectation file not found: %s\n' "$expectation_file" >&2
    return 1
  fi

  while IFS= read -r line || [[ -n "$line" ]]; do
    if [[ -z "$line" || "$line" == \#* ]]; then
      continue
    fi
    if [[ "$line" != "$step_name".* ]]; then
      continue
    fi

    matched=1
    field="${line#"${step_name}".}"
    field="${field%%=*}"
    expected="${line#*=}"
    actual="$(lcc_hw_summary_get "$summary_file" "$field")"
    if ! lcc_hw_value_matches "$actual" "$expected"; then
      printf 'step contract mismatch step=%s field=%s expected=%s actual=%s\n' \
        "$step_name" "$field" "$expected" "$actual" >&2
      failed=1
    fi
  done <"$expectation_file"

  if [[ "$matched" -eq 0 ]]; then
    printf 'missing step contract for %s in %s\n' "$step_name" \
      "$expectation_file" >&2
    return 1
  fi

  return "$failed"
}

lcc_hw_append_run_summary() {
  local run_summary_file="$1"
  local step_name="$2"
  local summary_file="$3"

  {
    printf '[%s]\n' "$step_name"
    cat "$summary_file"
    printf '\n'
  } >>"$run_summary_file"
}

lcc_hw_write_summary() {
  local state_file="$1"
  local summary_file="$2"
  local command_rc="$3"
  local state_rc="$4"
  local state_contract="not-captured"
  local missing_keys=""

  if [[ "$state_rc" -eq 0 ]]; then
    missing_keys="$(lcc_hw_collect_missing_state_keys "$state_file")"
    if [[ -n "$missing_keys" ]]; then
      state_contract="missing:${missing_keys}"
    else
      state_contract="ok"
    fi
  fi

  {
    printf 'command_exit_code=%s\n' "$command_rc"
    printf 'state_capture=%s\n' "$([[ "$state_rc" -eq 0 ]] && printf 'ok' || printf 'failed')"
    printf 'state_contract=%s\n' "$state_contract"
    printf 'backend=%s\n' "$(lcc_hw_json_get "$state_file" "backend")"
    printf 'backend_selected=%s\n' \
      "$(lcc_hw_json_get "$state_file" "backend_selected")"
    printf 'backend_fallback_reason=%s\n' \
      "$(lcc_hw_json_get "$state_file" "backend_fallback_reason")"
    printf 'effective_source=%s\n' \
      "$(lcc_hw_json_get_effective_meta_key "$state_file" "source")"
    printf 'effective_refresh=%s\n' \
      "$(lcc_hw_json_get_effective_meta_key "$state_file" "freshness")"
    printf 'execution_read_state=%s\n' \
      "$(lcc_hw_json_get_object_key "$state_file" "execution" "read_state")"
    printf 'execution_apply_profile=%s\n' \
      "$(lcc_hw_json_get_object_key "$state_file" "execution" "apply_profile")"
    printf 'execution_apply_mode=%s\n' \
      "$(lcc_hw_json_get_object_key "$state_file" "execution" "apply_mode")"
    printf 'execution_apply_power_limits=%s\n' \
      "$(lcc_hw_json_get_object_key "$state_file" "execution" "apply_power_limits")"
    printf 'execution_apply_fan_table=%s\n' \
      "$(lcc_hw_json_get_object_key "$state_file" "execution" "apply_fan_table")"
    printf 'hardware_write=%s\n' \
      "$(lcc_hw_json_get "$state_file" "hardware_write")"
    printf 'last_apply_stage=%s\n' \
      "$(lcc_hw_json_get "$state_file" "last_apply_stage")"
    printf 'last_apply_error=%s\n' \
      "$(lcc_hw_json_get "$state_file" "last_apply_error")"
    printf 'last_apply_backend=%s\n' \
      "$(lcc_hw_json_get "$state_file" "last_apply_backend")"
    printf 'last_apply_hardware_write=%s\n' \
      "$(lcc_hw_json_get "$state_file" "last_apply_hardware_write")"
    printf 'thermal_cpu_temp_c=%s\n' \
      "$(lcc_hw_json_get_object_key "$state_file" "thermal" "cpu_temp_c")"
    printf 'thermal_gpu_temp_c=%s\n' \
      "$(lcc_hw_json_get_object_key "$state_file" "thermal" "gpu_temp_c")"
    printf 'thermal_cpu_fan_rpm=%s\n' \
      "$(lcc_hw_json_get_object_key "$state_file" "thermal" "cpu_fan_rpm")"
    printf 'thermal_gpu_fan_rpm=%s\n' \
      "$(lcc_hw_json_get_object_key "$state_file" "thermal" "gpu_fan_rpm")"
    printf 'thermal_vendor_fan_level=%s\n' \
      "$(lcc_hw_json_get_object_key "$state_file" "thermal" "vendor_fan_level")"
    printf 'transaction_state=%s\n' \
      "$(lcc_hw_json_get_object_key "$state_file" "transaction" "state")"
    printf 'transaction_operation=%s\n' \
      "$(lcc_hw_json_get_object_key "$state_file" "transaction" "operation")"
    printf 'transaction_stage=%s\n' \
      "$(lcc_hw_json_get_object_key "$state_file" "transaction" "stage")"
    printf 'transaction_last_error=%s\n' \
      "$(lcc_hw_json_get_object_key "$state_file" "transaction" "last_error")"
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
      -n "$lines" --no-pager -o short-iso | tee "$output_file" >/dev/null
    return 0
  fi

  sudo journalctl -u "$service_name" -n "$lines" --no-pager -o short-iso \
    | tee "$output_file" >/dev/null
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
