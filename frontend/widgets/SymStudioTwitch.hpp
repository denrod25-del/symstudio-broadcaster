#pragma once

// Shared SymStudio Twitch app credentials. The client ID is a PUBLIC identifier
// (sent in every API request) — safe to embed. Device-code OAuth uses a public
// client, so no secret/backend is needed. One source of truth for all Twitch
// features (Stream Info, Stream Tracker, future EventSub).
#define SYMSTUDIO_TWITCH_CLIENT_ID "kq1ey23g2l11h44tnclrmz34b1k7h7"

// Current (channel:manage:broadcast) + next-milestone EventSub read scopes, so
// users consent once and EventSub needs no re-login.
#define SYMSTUDIO_TWITCH_SCOPES \
	"channel:manage:broadcast moderator:read:followers channel:read:subscriptions bits:read"
