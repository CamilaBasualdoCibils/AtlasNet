#pragma once

class NH_EntityAuthorityTracker;

// Writes tracker telemetry into AuthorityManifest format.
class SH_TelemetryPublisher
{
  public:
	void PublishFromTracker(const NH_EntityAuthorityTracker& tracker) const;
};
