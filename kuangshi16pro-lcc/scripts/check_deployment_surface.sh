#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
project_dir="$repo_root/kuangshi16pro-lcc"

readme="$project_dir/README.md"
docs_index="$project_dir/docs/README.md"
architecture_doc="$project_dir/docs/architecture.md"
dbus_api_doc="$project_dir/docs/dbus-api.md"
deployment_doc="$project_dir/docs/deployment-surface.md"
roadmap_doc="$project_dir/docs/roadmap.md"
systemd_unit="$project_dir/systemd/lccd.service"
dbus_service="$project_dir/systemd/dbus-io.github.semcosm.Lcc1.service"
bus_config="$project_dir/dbus/io.github.semcosm.Lcc1.conf"
polkit_policy="$project_dir/dbus/io.github.semcosm.Lcc1.policy"
policy_header="$project_dir/src/dbus/policy.h"
server_source="$project_dir/src/dbus/server.c"
client_source="$project_dir/src/cli/dbus_client.c"
manager_xml="$project_dir/src/dbus/introspection/io.github.semcosm.Lcc1.Manager.xml"
fan_xml="$project_dir/src/dbus/introspection/io.github.semcosm.Lcc1.Fan.xml"
power_xml="$project_dir/src/dbus/introspection/io.github.semcosm.Lcc1.Power.xml"

bus_name="io.github.semcosm.Lcc1"
object_path="/io/github/semcosm/Lcc1"
service_name="lccd.service"
service_exec="/usr/lib/kuangshi16pro-lcc/lccd --system"
dbus_system_service_name="io.github.semcosm.Lcc1.service"
systemd_unit_install_path="/usr/lib/systemd/system/lccd.service"
dbus_service_install_path="/usr/share/dbus-1/system-services/io.github.semcosm.Lcc1.service"
bus_config_install_path="/usr/share/dbus-1/system.d/io.github.semcosm.Lcc1.conf"
polkit_policy_install_path="/usr/share/polkit-1/actions/io.github.semcosm.Lcc1.policy"
license_install_path="/usr/share/licenses/kuangshi16pro-lcc/LICENSE"
man1_install_path="/usr/share/man/man1/lccctl.1"
man8_install_path="/usr/share/man/man8/lccd.8"

fail() {
  printf 'deployment-surface check failed: %s\n' "$*" >&2
  exit 1
}

require_line() {
  local file="$1"
  local needle="$2"

  if ! grep -Fq -- "$needle" "$file"; then
    fail "$file is missing: $needle"
  fi
}

reject_line() {
  local file="$1"
  local needle="$2"

  if grep -Fq -- "$needle" "$file"; then
    fail "$file still contains stale text: $needle"
  fi
}

check_bus_config_allow() {
  local interface_name="$1"
  local member_name="$2"

  if ! awk -v iface="$interface_name" -v member="$member_name" '
    BEGIN {
      pattern_iface = "send_interface=\"" iface "\""
      pattern_member = "send_member=\"" member "\""
    }
    /<allow / {
      block = $0
      if ($0 ~ /\/>/) {
        if (index(block, pattern_iface) && index(block, pattern_member)) {
          found = 1
        }
        block = ""
        collecting = 0
        next
      }
      collecting = 1
      next
    }
    collecting {
      block = block " " $0
      if ($0 ~ /\/>/) {
        if (index(block, pattern_iface) && index(block, pattern_member)) {
          found = 1
        }
        block = ""
        collecting = 0
      }
    }
    END {
      exit(found ? 0 : 1)
    }
  ' "$bus_config"; then
    fail "$bus_config is missing allow rule for $interface_name.$member_name"
  fi
}

check_method_contract() {
  local interface_name="$1"
  local member_name="$2"
  local signature_doc="$3"
  local xml_file="$4"
  local action_id="${5:-}"

  require_line "$deployment_doc" "$signature_doc"
  require_line "$dbus_api_doc" "$signature_doc"
  require_line "$server_source" "\"$interface_name\""
  require_line "$server_source" "SD_BUS_METHOD(\"$member_name\""
  require_line "$client_source" "\"$interface_name\""
  require_line "$xml_file" "<method name=\"$member_name\">"
  check_bus_config_allow "$interface_name" "$member_name"

  if [[ -n "$action_id" ]]; then
    require_line "$deployment_doc" "\`$interface_name.$member_name\` -> \`$action_id\`"
    require_line "$dbus_api_doc" "\`$interface_name.$member_name\` -> \`$action_id\`"
    require_line "$xml_file" "AuthorizationAction\" value=\"$action_id\""
    require_line "$polkit_policy" "<action id=\"$action_id\">"
    require_line "$policy_header" "$action_id"
  fi
}

require_line "$docs_index" "deployment-surface.md"
require_line "$architecture_doc" "docs/deployment-surface.md"

require_line "$deployment_doc" "bus name: \`$bus_name\`"
require_line "$deployment_doc" "object path: \`$object_path\`"
require_line "$deployment_doc" "systemd service: \`$service_name\`"
require_line "$deployment_doc" "D-Bus system service name: \`$dbus_system_service_name\`"
require_line "$deployment_doc" "\`ExecStart=$service_exec\`"
require_line "$deployment_doc" "\`$systemd_unit_install_path\`"
require_line "$deployment_doc" "\`Name=$bus_name\` and \`SystemdService=$service_name\`"
require_line "$deployment_doc" "\`$dbus_service_install_path\`"
require_line "$deployment_doc" "\`$bus_config_install_path\`"
require_line "$deployment_doc" "\`$polkit_policy_install_path\`"
require_line "$deployment_doc" "\`$license_install_path\`"
require_line "$deployment_doc" "\`$man1_install_path\`"
require_line "$deployment_doc" "\`$man8_install_path\`"

require_line "$dbus_api_doc" "bus name: \`$bus_name\`"
require_line "$dbus_api_doc" "object path: \`$object_path\`"

require_line "$systemd_unit" "BusName=$bus_name"
require_line "$systemd_unit" "ExecStart=$service_exec"
require_line "$dbus_service" "Name=$bus_name"
require_line "$dbus_service" "SystemdService=$service_name"
require_line "$bus_config" "<allow own=\"$bus_name\"/>"
require_line "$bus_config" "send_interface=\"org.freedesktop.DBus.Introspectable\""
require_line "$bus_config" "send_interface=\"org.freedesktop.DBus.Peer\""

require_line "$server_source" "\"$bus_name\""
require_line "$server_source" "\"$object_path\""
require_line "$client_source" "\"$bus_name\""
require_line "$client_source" "\"$object_path\""

check_method_contract \
  "io.github.semcosm.Lcc1.Manager" \
  "GetCapabilities" \
  "\`io.github.semcosm.Lcc1.Manager.GetCapabilities() -> s\`" \
  "$manager_xml"
check_method_contract \
  "io.github.semcosm.Lcc1.Manager" \
  "GetState" \
  "\`io.github.semcosm.Lcc1.Manager.GetState() -> s\`" \
  "$manager_xml"
check_method_contract \
  "io.github.semcosm.Lcc1.Manager" \
  "SetMode" \
  "\`io.github.semcosm.Lcc1.Manager.SetMode(mode_name: s)\`" \
  "$manager_xml" \
  "io.github.semcosm.Lcc1.set-mode"
check_method_contract \
  "io.github.semcosm.Lcc1.Manager" \
  "SetProfile" \
  "\`io.github.semcosm.Lcc1.Manager.SetProfile(profile_name: s)\`" \
  "$manager_xml" \
  "io.github.semcosm.Lcc1.set-profile"
check_method_contract \
  "io.github.semcosm.Lcc1.Fan" \
  "ApplyFanTable" \
  "\`io.github.semcosm.Lcc1.Fan.ApplyFanTable(table_name: s)\`" \
  "$fan_xml" \
  "io.github.semcosm.Lcc1.set-fan-table"
check_method_contract \
  "io.github.semcosm.Lcc1.Power" \
  "SetPowerLimits" \
  "\`io.github.semcosm.Lcc1.Power.SetPowerLimits(pl1: y, pl2: y, pl4: y, tcc_offset: y, has_pl1: b, has_pl2: b, has_pl4: b, has_tcc_offset: b)\`" \
  "$power_xml" \
  "io.github.semcosm.Lcc1.set-power-limits"

if [[ "$(grep -c 'send_member=' "$bus_config")" -ne 8 ]]; then
  fail "$bus_config does not expose exactly the stable v1 methods plus Introspect/Ping"
fi

if [[ "$(grep -ch '<method name=' "$manager_xml" "$fan_xml" "$power_xml" | awk '{sum += $1} END {print sum}')" -ne 6 ]]; then
  fail "introspection XML no longer exposes exactly six stable v1 methods"
fi

if [[ "$(grep -c '<action id=' "$polkit_policy")" -ne 4 ]]; then
  fail "$polkit_policy does not define exactly four mutating action ids"
fi

require_line "$readme" "installable systemd, D-Bus, and Polkit assets"
require_line "$readme" "validates the installed system-bus \`lccd\`"
reject_line "$readme" "scaffold"
reject_line "$readme" "skeleton"
reject_line "$architecture_doc" "scaffold"
reject_line "$roadmap_doc" "PR9 completed"
reject_line "$roadmap_doc" "minimal Linux command set"

printf 'deployment-surface check passed\n'
