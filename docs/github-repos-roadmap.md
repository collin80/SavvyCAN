# SavvyCAN GitHub repos: loading and saving logs and DBCs

Scope: opening and saving log files and DBC/LDF files stored in GitHub repos, from inside SavvyCAN (Tom-evnut fork).
Written against `fork-ci` (2026-10-25). Each phase branches from and merges into `fork-ci`.

---

## 1. Where it stands (2026-10-25)

| Phase | State |
|---|---|
| 0: file dialogs split from loading and saving | Done, merged into `fork-ci` (`8e00c02`) |
| 1: load logs and DBC files from GitHub | Done, merged into `fork-ci` (`ecda015`); CI packages carry the TLS plugins. Tested against a fake GitHub only |
| 1b: sign-in for everyone (the SavvyCAN GitHub App built in), shared repo list, EV-CANlogs preloaded | Done, merged into `fork-ci`; signing in and loading from `Tom-evnut/EV-CANlogs` confirmed by the user on Windows |
| 2: save logs and DBC files, create repos | Done, merged into `fork-ci`. Tested against a fake GitHub; the real round trip on `Tom-evnut/SavvyCan_LogTest` is next |
| 3, 4 | Not started |

---

## 2. Decisions

| Question | Decision |
|---|---|
| Sign-in | "Sign in with GitHub" through the SavvyCAN GitHub App ([github.com/apps/savvycan](https://github.com/apps/savvycan), Tom-evnut's), built in so users register nothing; a pasted personal access token, or the user's own app, as alternatives |
| Starting repo | `Tom-evnut/EV-CANlogs` (public) is in every new user's repo list |
| Hosts | github.com only. The API base URL is a hidden setting so GitHub Enterprise can come later, untested |
| Log sizes | Mostly under 50 MB: the Contents API is enough; the Git Data API and LFS wait for phase 4 |
| Committing | Straight to the selected branch by default; "new branch and open a pull request" is an option when saving |
| Saved repos | One list for logs and DBC files: most repos hold both |
| New repos | Can be created from the save dialog (phase 2), private by default |
| Test repo | A new private scratch repo (e.g. `tomdebree/savvycan-github-test`) with copies of `test/lin-data` files |

---

## 3. How it fits the current code

Every log loader and saver in `FrameFileIO` takes a file path (`autoDetectLoadFile(QString, ...)`,
`saveNativeCSVFile(QString, ...)` and so on), and so do `DBCFile::loadFile` and `DBCFile::saveFile`. Only the
`QFileDialog` picks the path. So:

- **Load:** download the file into a local cache folder, then run the existing loader on it.
- **Save:** write the file into the cache with the existing saver, then upload it as a commit.

The log formats and the DBC/LDF parser don't change. Qt Network is already linked (`SavvyCAN.pro`,
`CMakeLists.txt`), so there's no new library and no need for git or libgit2 on the user's machine.

---

## 4. Design

### 4.1 `GitHubClient` (`github/githubclient.{h,cpp}`)

A wrapper around `QNetworkAccessManager` and GitHub's REST API (`https://api.github.com`). Built in phase 1 unless
marked otherwise:

| Use | Request |
|---|---|
| Account | `GET /user` |
| Repo check, default branch | `GET /repos/{owner}/{repo}` |
| Branches | `GET /repos/{owner}/{repo}/branches?per_page=100`, following the `Link` header's next page |
| File tree | `GET /repos/{owner}/{repo}/git/trees/{commit sha}?recursive=1` (whole tree in one request; the branch's commit SHA, so branch names with `/` work) |
| Download | `GET /repos/{owner}/{repo}/contents/{path}?ref={commit sha}` with `Accept: application/vnd.github.raw` |
| Save (phase 2) | `PUT /repos/{owner}/{repo}/contents/{path}`: base64 content, commit message, branch, and the blob SHA of the version it replaces |
| New branch (phase 2) | `POST /repos/{owner}/{repo}/git/refs` from the selected branch's head |
| Pull request (phase 2) | `POST /repos/{owner}/{repo}/pulls` |
| Owners for a new repo (phase 2) | `GET /user/orgs`: the user's own account plus their organisations |
| New repo (phase 2) | `POST /user/repos`, or `POST /orgs/{org}/repos` |

- A save whose SHA no longer matches gets **409 Conflict**: the user can overwrite, save under a new name, or cancel.
- Errors are shown in plain words: 401 (sign in again), 403 or 429 with `X-RateLimit-Remaining: 0` (rate limit, with
  the reset time), 404 (no access or no such file), anything else with GitHub's own message.
- Public repos work without signing in, at GitHub's 60 requests an hour.
- Deleting the client aborts its requests without calling their callbacks.

### 4.2 Sign-in

**File > GitHub Account...** (also reachable from the browser dialog) signs in. `GitHubClient` only sees a token and
sends it as `Authorization: Bearer`.

- **Sign in with GitHub (device flow):** one click. SavvyCAN asks `https://github.com/login/device/code` for a code,
  copies it to the clipboard and opens `https://github.com/login/device`, where the user pastes it and approves; it
  polls `https://github.com/login/oauth/access_token` at the interval GitHub gives until then. Only a client ID is
  needed, never a client secret.
  - **The SavvyCAN GitHub App** is built in (`GITHUB_APP_CLIENT_ID`, `GITHUB_APP_SLUG` in `githubsettings.cpp`), so
    nobody else registers anything. Its user tokens reach public repos and the repos it's installed on: **Choose
    Repos...** in the dialog opens `https://github.com/apps/savvycan/installations/new`, and a repo GitHub won't
    show links there too. App settings: "Enable Device Flow" ticked, "Expire user authorization tokens" unticked
    (SavvyCAN doesn't refresh tokens yet), webhook off, installable on any account; repository permissions
    *Contents: read and write*, *Pull requests: read and write* (phase 2's pull request option), *Metadata:
    read-only*, and *Administration: read and write* only once repos are to be created from SavvyCAN (section 4.7;
    everyone who installed the app is then asked to accept the new permission).
  - **The user's own app** ("Use my own GitHub App or OAuth App"), for forks or organisations: its client ID is
    entered there. An OAuth App goes by scope: `repo` reaches all of the user's private repos, `public_repo` (the
    "Include private repos" box unticked) only public ones; the box is hidden for GitHub Apps, which ignore scopes.
- **Personal access token:** pasted in, for users who want access limited to chosen repos (a fine-grained token with
  *Contents: read and write* and, for pull requests, *Pull requests: read and write*; *Administration: read and
  write* for creating repos). A token is kept only once `GET /user` accepts it.
- Either token is stored with `SimpleCrypt`, the same way as the remote password in `MainSettingsDialog`. That only
  hides it from a casual look, and the dialog says so. "Sign Out" deletes it.
- The signed-in account's name (`GET /user`) is shown so it's clear which account is in use.

### 4.3 Saved repos

One list in `QSettings` (`GitHub/Repos`), shared by logs and DBC files since most repos hold both: owner/repo and the
branch picked last. Added in the browser dialog from `owner/repo` or a github.com link, after
`GET /repos/{owner}/{repo}` checks it. Removing a repo (after a question) takes it off the list for both; nothing
changes on GitHub. Repos the first version saved for logs or DBC files only are in the one list now. Until the list
is first saved it holds `Tom-evnut/EV-CANlogs`, so new users have logs and DBC files to load straight away; a list
the user empties stays empty.

### 4.4 Local cache

- Files go to `QStandardPaths::CacheLocation/github/<owner>/<repo>/<branch>/<path>`, keeping the real file name so
  autodetect still sees the extension. The branch name is percent-encoded so a `/` in it can't make two branches
  share a folder; paths with `..`, empty parts or absolute paths are refused.
- A `<file>.origin.json` next to each file records repo, branch, path and blob SHA; saving back reads it.
- "Clear Downloaded Files" in the GitHub Account dialog empties it. A size limit (oldest files first) is still to do.

### 4.5 `GitHubBrowserDialog`

One dialog for logs and DBCs:

- Account line, repo picker (Add..., Remove), branch picker, file type, search box, and the file tree (folders, file
  sizes). The repos and their branches are the same for logs and DBC files; the last repo and file type picked are
  remembered separately for each.
- **Open mode (phase 1):** pick a file; it's downloaded with a progress dialog that can be cancelled. Files over
  100 MB and Git LFS pointer files are refused with a message.
- **Save mode (phase 2):** the tree shows every folder; clicking a folder picks it, clicking a file picks its folder
  and name (to replace it). Then the file type, folder, file name (it follows the file type: "run.csv" becomes
  "run.asc"), an optional commit message ("Add/Update <file> from SavvyCAN" otherwise), and "Save to a new branch and
  open a pull request" with the branch name (`savvycan/<file>-<date>-<time>` suggested) and pull request title.
  **New Repo...** sits next to the repo picker (section 4.7). Saving needs a sign-in.
  - **How it saves:** SavvyCAN's own saver (a writer the caller hands over) writes the file into a temporary folder;
    it's uploaded with `PUT contents`, and only then stored in the cache with its new blob SHA.
  - **Which version it replaces:** the one loaded, when saving back to the same repo, branch and path; otherwise
    the one in the file list, after asking "Replace it?"; none for a new file.
  - **Conflicts:** when GitHub has another version (409, or 422 "sha wasn't supplied"), the user picks
    **Overwrite** (replaces whatever is there now), **Save Under a New Name** ("run (2).csv", nothing saved yet) or
    Cancel.
  - **New branch:** made from the branch picked, then the file is saved there and a pull request opened into the
    branch picked. If the save fails, the new branch is deleted again.
  - **Sizes:** over 50 MB the user is asked first; over 100 MB GitHub refuses, so SavvyCAN does too.
  - Afterwards a message gives the link to the commit or the pull request.

### 4.6 Refactor first (phase 0, done)

- `FrameFileIO::loadFilters()`/`saveFilters()` with `loadFrameFileAs(path, filterIndex, frames)` and
  `saveFrameFileAs(path, filterIndex, frames)`.
- `DBCHandler::loadFilters()`/`saveFilters()` with `loadFileAs(path, filterIndex)` and `saveDBCFileAs(idx, path)`.

### 4.7 Creating a repo (phase 2)

From the save dialog, so a log or DBC file can go into a repo that doesn't exist yet:

- **New Repo...** opens a small dialog: owner (the signed-in account, or one of their organisations from
  `GET /user/orgs`), name, optional description, and **Private** (ticked by default).
- SavvyCAN creates it with `auto_init: true`, so it starts with a README and a default branch to commit to, adds it
  to the saved repos and selects it with its default branch; the user then saves into it.
- Errors in plain words:
  - 422 "name already exists on this account": pick another name, or add the existing repo instead;
  - 403/404: the sign-in can't create repos. The message names what's missing: *Administration: read and write*
    for a GitHub App or fine-grained token, the `repo` scope (or `public_repo` for a public repo) for an OAuth App;
  - an organisation can also forbid its members from creating repos.
- **GitHub App installed on selected repos only:** still to check on real GitHub whether a repo the app creates is
  added to that installation. If it isn't, reading its branches fails with "not found", and the browser's hint links
  to the installation page.
- **Signed out:** the button is disabled (creating needs an account).

---

## 5. Logs

- **File menu:** "Load Log File from GitHub..." (phase 1) and "Save Log File to GitHub..." (phase 2).
- **Load:** pick a file, download it, load it as the chosen file type (autodetect by default), with the same
  "salvage what could be loaded?" question as other loads. The file status shows `github:owner/repo/path`.
- **Save:** the chosen saver writes the frames, then they're uploaded. A log that came from GitHub (and is still the
  one shown) suggests its own repo, branch and path and passes its SHA, so the conflict check works; any other log
  suggests its own file name. Afterwards the file status shows the GitHub path.
- **Size:** above 50 MB the save dialog asks first (GitHub warns at 50 MB and refuses 100 MB without LFS) and
  suggests a more compact file type.

---

## 6. DBCs

- **DBC File Manager (`DBCLoadSaveWindow`):** "Load from GitHub" (phase 1) and "Save to GitHub" (phase 2).
- **Load:** download, then `DBCHandler::loadFileAs(path, type)`. The cached file's `.origin.json` says where it
  came from. DBC files loaded from GitHub reload from the cache at the next start.
- **Save:** the DBC picked in the list. One loaded from GitHub suggests its own path, with the conflict check. LDF
  files are saved as `.dbc`, as the local save does. Afterwards the DBC goes by its cached copy (so it reloads from
  there at the next start); if nothing was saved, it keeps its old name and unsaved-changes flag.
- **Startup (phase 3):** at startup the cached copy loads at once; a quiet check against GitHub then offers to reload
  if the file changed there. Nothing is replaced without asking.
- **Later, optional:** load every `.dbc` in a repo folder, with buses assigned from a small `savvycan.json` in the
  repo.

---

## 7. Phases

| Phase | Content | Branch |
|---|---|---|
| 0 (done) | Split the file dialogs from the load/save code in `FrameFileIO` and `DBCHandler`; Candump save result fixed; `TestFileDialogSplit` | `github-phase0-refactor` |
| 1 (done) | `GitHubClient` read side, device flow and token sign-in, cache, browser and account dialogs; **load** logs and DBCs; TLS plugins in the fork CI packages; `TestGitHub` | `github-phase1-load` |
| 2 (done) | **Save** logs and DBCs through the Contents API, conflict handling, new branch and pull request option, **creating a repo** (private by default) | `github-phase2-save` |
| 3 | DBC files from GitHub checked against GitHub at startup, recent GitHub files, cache size limit | `github-phase3-dbc-sync` |
| 4 | Git Data API for large files, LFS, expiring GitHub App tokens (refresh), OS keychain storage, Enterprise | later |

---

## 8. Testing

- `unit_tests/test_github.cpp` (`TestGitHub`) runs against a local `QTcpServer` faking GitHub. Phase 1 covers
  headers, branch paging, tree parsing, raw download with encoded paths, 401/403/404/500 and the rate limit, an
  unreachable server, a client deleted mid-request, the device flow (`authorization_pending`, `slow_down`,
  `access_denied`, `expired_token`, device flow disabled), the cache, settings, and both dialogs. With
  `SAVVYCAN_SCREENSHOTS` set it saves pictures of the dialogs.
- Phase 2 adds a fake GitHub that keeps state (repos, branches, files, commit SHAs) and covers: the client's save,
  file SHA, branch, pull request, organisation and repo calls; saving a new file; saving back, the conflict choices
  (Overwrite, Save Under a New Name) and replacing another file; a new branch with a pull request, and its removal
  when the save fails; creating repos (account, organisation, name taken, no permission); the name following the
  file type; no saving signed out.
- `unit_tests/test_file_dialog_split.cpp` (`TestFileDialogSplit`) saves and reloads each log type and a DBC file.
- Round trip against the scratch repo: load a log and a DBC, save them back, compare byte for byte; edit the file on
  github.com in between to check the conflict prompt. Repo creation is tried with a throwaway name under the user's
  own account.
- Save, conflict, pull request and repo creation tests only ever touch the scratch repo (or the throwaway one).

---

## 9. Risks

- **TLS in packaged builds:** solved in phase 1. The Windows package ships Qt's `tls` plugins (Schannel) and the
  AppImage the OpenSSL backend, which uses the system's OpenSSL 3 (checked against api.github.com, 2026-10-25).
  Distributions without OpenSSL 3 (older than Ubuntu 22.04) get no HTTPS from the AppImage.
- **Token storage:** `SimpleCrypt` isn't real protection; phase 4 moves tokens to the OS keychain.
- **Broad sign-ins:** an OAuth App token with `repo` reaches every private repo of the user; a GitHub App limits it
  to the repos it's installed on. *Administration: read and write*, needed to create repos, also lets the token
  change repo settings, so it's optional and documented as such.

---

## 10. Needed from the user

- Done: the SavvyCAN GitHub App is registered (2026-10-25) and signing in works.
- Done: the scratch repo is `Tom-evnut/SavvyCan_LogTest` (2026-10-25); the save tests only ever write there.
- Done: *Administration: read and write* added to the SavvyCAN app (2026-10-25); installation owners accept it.
- Next: try saving on real GitHub (a log, a DBC, a conflict, a new branch with a pull request, New Repo...).
