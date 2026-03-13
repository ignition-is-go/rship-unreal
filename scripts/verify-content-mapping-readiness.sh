#!/bin/bash
# Fail-fast readiness gate for content mapping runtime material availability.
# Usage: ./scripts/verify-content-mapping-readiness.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

candidate_files=(
  "$REPO_ROOT/Plugins/RshipMapping/Content/Materials/MI_RshipContentMapping.uasset"
  "$REPO_ROOT/Plugins/RshipMapping/Content/Materials/M_RshipContentMapping.uasset"
  "$REPO_ROOT/Plugins/RshipExec/Content/Materials/MI_RshipContentMapping.uasset"
  "$REPO_ROOT/Plugins/RshipExec/Content/Materials/M_RshipContentMapping.uasset"
  "$REPO_ROOT/Content/Rship/Materials/MI_RshipContentMapping.uasset"
  "$REPO_ROOT/Content/Rship/Materials/M_RshipContentMapping.uasset"
)

found_candidates=()
for candidate in "${candidate_files[@]}"; do
  if [[ -f "$candidate" ]]; then
    found_candidates+=("$candidate")
  fi
done

total_uassets="$(find "$REPO_ROOT" -type f -name '*.uasset' | wc -l | tr -d ' ')"
settings_override="$(rg -n "ContentMappingMaterialPath\\s*=" "$REPO_ROOT/Config" 2>/dev/null || true)"

echo "== Content Mapping Readiness =="
echo "Repo root: $REPO_ROOT"
echo "Total .uasset files: $total_uassets"
if [[ -n "$settings_override" ]]; then
  echo "Configured ContentMappingMaterialPath lines:"
  echo "$settings_override"
else
  echo "Configured ContentMappingMaterialPath lines: (none found in Config/)"
fi

if [[ "${#found_candidates[@]}" -gt 0 ]]; then
  echo "Resolved candidate mapping material assets:"
  for candidate in "${found_candidates[@]}"; do
    echo "  - $candidate"
  done
  echo "PASS: content-mapping material candidates are present."
  exit 0
fi

cat <<'EOF'
FAIL: no mapping material assets found in any runtime candidate location.
Image-on-surface rendering cannot succeed without a valid mapping material asset.

Expected at least one of:
  - Plugins/RshipMapping/Content/Materials/MI_RshipContentMapping.uasset
  - Plugins/RshipMapping/Content/Materials/M_RshipContentMapping.uasset
  - Plugins/RshipExec/Content/Materials/MI_RshipContentMapping.uasset
  - Plugins/RshipExec/Content/Materials/M_RshipContentMapping.uasset
  - Content/Rship/Materials/MI_RshipContentMapping.uasset
  - Content/Rship/Materials/M_RshipContentMapping.uasset
EOF

exit 2
