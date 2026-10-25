# Illustrated onboarding — iOS 27

Three native SwiftUI pages replace the previous text-led introduction. Artwork comes from bundled transparent PNGs, with adaptive native backgrounds, typography, buttons and pagination. Horizontal swiping, Continue and Back all navigate the same selection. The last page offers connection or a complete demo; Skip remains available throughout.

The guide remains available under Settings → Rivedi introduzione. Completion persistence, mode isolation, saved themes and the home-routing callback are unchanged. No Bluetooth permission or discovery starts merely by viewing the introduction. The imagery is decorative for VoiceOver; titles, progress, navigation and actions remain accessible. The indicator supports VoiceOver adjustment. Programmatic page motion is disabled when Reduce Motion is enabled. Accessibility text sizes get smaller artwork and vertically scrolling content without clamping fonts.

Design guidance: [Apple HIG — Onboarding](https://developer.apple.com/design/human-interface-guidelines/onboarding): brief, optional, focused on the product, available again in Settings, and media bundled locally.

See [ASSETS.md](ASSETS.md) for the exact Image Gen prompts and final asset locations.

## Validation

- Light appearance: first launch and replay (including swipe, Back, persisted completion and same-mode home routing), connection choice and large-text Skip: 3 cases passed, zero failures. Result: `/tmp/ag-illustrated-onboarding-ui.xcresult`.
- Dark appearance: first launch/replay and maximum accessibility text completion: 2 cases passed. Final verification after fixing primary-button wrapping repeated these 2 cases with zero failures: `/tmp/ag-illustrated-onboarding-final-ui.xcresult`.
- Large accessibility text: both final actions remain tappable and the primary label wraps without ellipsis. Page content scrolls vertically; text size is never clamped. Screenshot `dark/66-onboarding-start-large-text.png` records the corrected layout.
- Signed iPhone build succeeded: `/tmp/ag-illustrated-onboarding-final-iphone.log`. Final install succeeded on retry: `/tmp/ag-illustrated-onboarding-final-install-retry.log`. The phone was locked, so final automatic launch was denied by iOS. No user data was reset.
- User simulator updated and launched into the introduction using the isolated UI-test preference namespace; normal app preferences remain intact.

All three normal-size pages were inspected in light and dark appearance. Screenshot provenance is in `light-captures.json` and `dark-captures.json`. The native bottom safe area, image transparency, text contrast and navigation were checked. The light screenshots predate the button-wrapping adjustment, which changes only the large-text button layout.
