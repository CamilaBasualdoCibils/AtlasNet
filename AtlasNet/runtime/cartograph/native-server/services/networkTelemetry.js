function decodeConnectionRow(row) {
  const hasShardId = row.length >= 14;
  const offset = hasShardId ? 1 : 0;

  return {
    shardId: hasShardId ? row[0] : null,
    IdentityId: row[offset],
    targetId: row[offset + 1],
    pingMs: Number(row[offset + 2]),
    inBytesPerSec: Number(row[offset + 3]),
    outBytesPerSec: Number(row[offset + 4]),
    inPacketsPerSec: Number(row[offset + 5]),
    pendingReliableBytes: Number(row[offset + 6]),
    pendingUnreliableBytes: Number(row[offset + 7]),
    sentUnackedReliableBytes: Number(row[offset + 8]),
    queueTimeUsec: Number(row[offset + 9]),
    qualityLocal: Number(row[offset + 10]),
    qualityRemote: Number(row[offset + 11]),
    state: row[offset + 12],
  };
}

function computeShardAverages(connections) {
  if (!connections || connections.length === 0) {
    return { inAvg: 0, outAvg: 0 };
  }

  let inSum = 0;
  let outSum = 0;

  for (const c of connections) {
    inSum += c.inBytesPerSec;
    outSum += c.outBytesPerSec;
  }

  return {
    inAvg: inSum / connections.length,
    outAvg: outSum / connections.length,
  };
}

function toStringRows(telemetryVec) {
  const rows = [];
  for (let i = 0; i < telemetryVec.size(); i += 1) {
    const rowVec = telemetryVec.get(i);
    const row = [];
    for (let j = 0; j < rowVec.size(); j += 1) {
      row.push(String(rowVec.get(j)));
    }
    rows.push(row);
  }
  return rows;
}

function readNetworkTelemetry(addon, networkTelemetry) {
  const {
    std_vector_std_string_,
    std_vector_std_vector_std_string__,
  } = addon;

  const idsVec = new std_vector_std_string_();
  const healthVec = new std_vector_std_string_();
  const telemetryVec = new std_vector_std_vector_std_string__();

  networkTelemetry.GetLivePingIDs(idsVec, healthVec);
  networkTelemetry.GetAllTelemetry(telemetryVec);

  const ids = [];
  const count = Math.min(idsVec.size(), healthVec.size());
  for (let i = 0; i < count; i += 1) {
    ids.push(String(idsVec.get(i)));
  }

  const rowsByShard = new Map();
  for (const row of toStringRows(telemetryVec)) {
    if (row.length < 13) {
      continue;
    }

    const decoded = decodeConnectionRow(row);
    const shardId = decoded.shardId ?? decoded.IdentityId;
    if (!rowsByShard.has(shardId)) {
      rowsByShard.set(shardId, []);
    }
    rowsByShard.get(shardId).push(decoded);
  }

  return ids.map((id) => {
    const connections = rowsByShard.get(id) ?? [];
    const { inAvg, outAvg } = computeShardAverages(connections);
    return {
      shardId: id,
      downloadKbps: inAvg,
      uploadKbps: outAvg,
      connections,
    };
  });
}

module.exports = {
  readNetworkTelemetry,
};
