/**
 * The archive the last real build produced, together with the group profile
 * that produced it. Publish, the Aminet upload and revoke all act on this
 * record; the profile travels with it because switching the dropdown after a
 * build must never send that archive out under another group's server, key
 * or numbering.
 */
export interface BuiltRelease {
  profileId: string;
  code: string;
  archivePath: string;
  // Frozen at build time: the form's media-embed rows can be edited or
  // cleared between Build and Publish, and Publish must send exactly what was
  // built and sent to the server.
  embedImagePaths: string[];
}

/**
 * The build that may still be published or uploaded while `currentProfileId`
 * is selected, or null when there is none or it belongs to another profile.
 */
export function builtReleaseFor(built: BuiltRelease | null, currentProfileId: string): BuiltRelease | null {
  return built !== null && built.profileId === currentProfileId ? built : null;
}
