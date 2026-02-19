#pragma once

class NH_EntityAuthorityTracker;

class SH_TelemetryPublisher
{
  public:
	void PublishFromTracker(const NH_EntityAuthorityTracker& tracker) const;
};
