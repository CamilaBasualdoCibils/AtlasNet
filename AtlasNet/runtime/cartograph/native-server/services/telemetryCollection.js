const { readNetworkTelemetry } = require('./networkTelemetry');
const { readEntityLedgersTelemetry } = require('./entityLedgerTelemetry');
const { readHeuristicShapes } = require('./heuristicShapes');
const { readTransferStateQueueTelemetry } = require('./transferStateQueueTelemetry');
const {
  readAuthorityTelemetryFromDatabase,
  readHeuristicClaimedOwnersFromDatabase,
  readNetworkTelemetryFromDatabase,
  readHeuristicShapesFromDatabase,
  readTransferManifestFromDatabase,
  readTransferStateQueueFromDatabase,
} = require('./databaseTelemetry');

async function collectNetworkTelemetry({ addon, networkTelemetry }) {
  const hasAddon = Boolean(
    addon &&
      networkTelemetry &&
      addon.std_vector_std_string_ &&
      addon.std_vector_std_vector_std_string__
  );
  if (hasAddon) {
    return readNetworkTelemetry(addon, networkTelemetry);
  }
  return readNetworkTelemetryFromDatabase();
}

async function collectAuthorityTelemetry({ addon, entityLedgersView }) {
  const hasAddon = Boolean(
    addon && entityLedgersView && addon.std_vector_EntityLedgerEntry_
  );
  if (hasAddon) {
    const ownerByBoundId = await readHeuristicClaimedOwnersFromDatabase();
    return readEntityLedgersTelemetry(addon, entityLedgersView, {
      ownerByBoundId,
    });
  }
  return readAuthorityTelemetryFromDatabase();
}

async function collectHeuristicShapes({ addon }) {
  const hasAddon = Boolean(
    addon && addon.HeuristicDraw && addon.std_vector_IBoundsDrawShape_
  );
  if (hasAddon) {
    return readHeuristicShapes(addon);
  }
  return readHeuristicShapesFromDatabase();
}

async function collectTransferManifest() {
  return readTransferManifestFromDatabase();
}

async function collectTransferStateQueue({ addon, transferStateQueueView }) {
  const hasAddon = Boolean(
    addon && transferStateQueueView && addon.std_vector_std_vector_std_string__
  );
  if (hasAddon) {
    return readTransferStateQueueTelemetry(addon, transferStateQueueView);
  }
  return readTransferStateQueueFromDatabase();
}

module.exports = {
  collectNetworkTelemetry,
  collectAuthorityTelemetry,
  collectHeuristicShapes,
  collectTransferManifest,
  collectTransferStateQueue,
};
