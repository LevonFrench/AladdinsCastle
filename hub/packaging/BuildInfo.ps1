# Pure provenance selection; no machine information is included.
function Get-PackageBuildRevision {
    param([string]$CiRevision, [string]$CheckoutRevision)
    $revision = if ($CiRevision) { $CiRevision } else { $CheckoutRevision }
    if ($revision -notmatch '^[a-fA-F0-9]{40}$') { throw 'Package source revision must be a full Git commit SHA.' }
    return $revision.ToLowerInvariant()
}
