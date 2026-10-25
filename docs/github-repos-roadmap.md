# SavvyCAN GitHub repos: loading and saving logs and DBCs

Scope: opening and saving log files and DBC/LDF files stored in GitHub repos, from inside SavvyCAN (Tom-evnut fork).
Written against `fork-ci` (2026-10-25). Each phase branches from and merges into `fork-ci`.

---

## 1. Where it stands (2026-10-25)

| Phase | State |
|---|---|
| 0: file dialogs split from loading and saving | Done, merged into `fork-ci` (`8e00c02`) |
| 1: load logs and DBC files from GitHub | Done, merged into `fork-ci` (`ecda015`); CI packages carry the TLS plugins. Tested against a fake GitHub only |
| 2: save logs and DBC files, create repos | Next |
| 3, 4 | Not started |

---

## 2. Decisions

| Question | Decision |
|---|---|
| Sign-in | Both from the first version: "Sign in with GitHub" (device flow, through an OAuth App or a GitHub App) and a pasted personal access token |
| Hosts | github.com only. The API base URL is a hidden setting so GitHub Enterprise can come later, untested |
| Log sizes | Mostly under 50 MB: the Contents API is enough; the Git Data API and LFS wait for phase 4 |
| Committing | Straight to the selected branch by default; "new branch and open a pull request" is an option when saving |
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

- **Sign in with GitHub (device flow):** SavvyCAN asks `https://github.com/login/device/code` for a code, shows it
  with a button that copies it and opens `https://github.com/login/device`, and polls
  `https://github.com/login/oauth/access_token` at the interval GitHub gives until the user approves. Only the app's
  client ID is needed, entered in the dialog (no client secret). Either kind of app works:
  - **OAuth App:** "Enable Device Flow" ticked. Scope `repo` reaches all of the user's private repos; `public_repo`
    (the dialog's "Include private repos" unticked) only public ones.
  - **GitHub App:** reaches only the repos it's installed on. Settings: "Enable Device Flow" ticked, "Expire user
    authorization tokens" unticked (SavvyCAN doesn't refresh tokens yet), webhook off; repository permissions
    *Contents: read and write*, *Pull requests: read and write* (phase 2's pull request option), *Metadata:
    read-only*, and *Administration: read and write* only if repos are to be created from SavvyCAN (section 4.7).
- **Personal access token:** pasted in, for users who want access limited to chosen repos (a fine-grained token with
  *Contents: read and write* and, for pull requests, *Pull requests: read and write*; *Administration: read and
  write* for creating repos). A token is kept only once `GET /user` accepts it.
- Either token is stored with `SimpleCrypt`, the same way as the remote password in `MainSettingsDialog`. That only
  hides it from a casual look, and the dialog says so. "Sign Out" deletes it.
- The signed-in account's name (`GET /user`) is shown so it's clear which account is in use.

### 4.3 Saved repos

A list in `QSettings` (`GitHub/Repos`): owner/repo, the branch picked last, and what it holds (logs, DBC files or
both). Added in the browser dialog from `owner/repo` or a github.com link, after `GET /repos/{owner}/{repo}` checks it.
Removing a repo only takes it off the list.

### 4.4 Local cache

- Files go to `QStandardPaths::CacheLocation/github/<owner>/<repo>/<branch>/<path>`, keeping the real file name so
  autodetect still sees the extension. The branch name is percent-encoded so a `/` in it can't make two branches
  share a folder; paths with `..`, empty parts or absolute paths are refused.
- A `<file>.origin.json` next to each file records repo, branch, path and blob SHA; saving back reads it.
- "Clear Downloaded Files" in the GitHub Account dialog empties it. A size limit (oldest files first) is still to do.

### 4.5 `GitHubBrowserDialog`

One dialog for logs and DBCs:

- Account line, repo picker (Add..., Remove), branch picker, file type, search box, and the file tree (folders, file
  sizes). The last repo and file type are remembered per kind.
- **Open mode (phase 1):** pick a file; it's downloaded with a progress dialog that can be cancelled. Files over
  100 MB and Git LFS pointer files are refused with a message.
- **Save mode (phase 2):** pick a folder, file name, format (logs), commit message, and a "Commit to a new branch and
  open a pull request" checkbox with branch name and PR title; a **New Repo...** button next to the repo picker
  (section 4.7). After saving, the commit or pull request link is shown.

### 4.6 Refactor first (phase 0, done)

- `FrameFileIO::loadFilters()`/`saveFilters()` with `loadFrameFileAs(path, filterIndex, frames)` and
  `saveFrameFileAs(path, filterIndex, frames)`.
- `DBCHandler::loadFilters()`/`saveFilters()` with `loadFileAs(path, filterIndex)` and `saveDBCFileAs(idx, path)`.

### 4.7 Creating a repo (phase 2)

From the save dialog, so a log or DBC file can go into a repo that doesn't exist yet:

- **New Repo...** opens a small dialog: owner (the signed-in account, or one of their organisations from
  `GET /user/orgs`), name, optional description, and **Private** (ticked by default).
- SavvyCAN creates it with `auto_init: true`, so it starts with a README and a default branch to commit to, adds it
  to the saved repos for this kind (logs or DBC files), selects it with its default branch, and carries on with the
  save.
- Errors in plain words:
  - 422 "name already exists on this account": pick another name, or add the existing repo instead;
  - 403/404: the sign-in can't create repos. The message names what's missing: *Administration: read and write*
    for a GitHub App or fine-grained token, the `repo` scope (or `public_repo` for a public repo) for an OAuth App;
  - an organisation can also forbid its members from creating repos.
- **GitHub App installed on selected repos only:** check whether GitHub adds a repo the app creates to that
  installation. If it doesn't, SavvyCAN can't commit to it until the user adds it to the installation; the dialog then
  says so and links to the installation's settings.
- **Signed out:** the button is disabled (creating needs an account).

---

## 5. Logs

- **File menu:** "Load Log File from GitHub..." (phase 1) and "Save Log File to GitHub..." (phase 2).
- **Load:** pick a file, download it, load it as the chosen file type (autodetect by default), with the same
  "salvage what could be loaded?" question as other loads. The file status shows `github:owner/repo/path`.
- **Save:** the chosen saver writes into the cache, then it's uploaded. A log that came from GitHub suggests its own
  path and passes its SHA, so the conflict check works.
- **Size:** above about 50 MB the save dialog warns (GitHub warns at 50 MB and refuses 100 MB without LFS) and
  suggests BLF or pcapng, which are much smaller than CSV.

---

## 6. DBCs

- **DBC File Manager (`DBCLoadSaveWindow`):** "Load from GitHub" (phase 1) and "Save to GitHub" (phase 2).
- **Load:** download, then `DBCHandler::loadFileAs(path, type)`. The cached file's `.origin.json` says where it
  came from. DBC files loaded from GitHub reload from the cache at the next start.
- **Save:** a DBC from GitHub commits back to its own path after asking for a commit message, with the conflict
  check. LDF files are saved as `.dbc`, as the local save does.
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
| 2 | **Save** logs and DBCs through the Contents API, 409 conflict handling, new branch and pull request option, **creating a repo** (private by default) | `github-phase2-save` |
| 3 | DBC files from GitHub checked against GitHub at startup, recent GitHub files, cache size limit | `github-phase3-dbc-sync` |
| 4 | Git Data API for large files, LFS, expiring GitHub App tokens (refresh), OS keychain storage, Enterprise | later |

---

## 8. Testing

- `unit_tests/test_github.cpp` (`TestGitHub`) runs against a local `QTcpServer` faking GitHub. Phase 1 covers
  headers, branch paging, tree parsing, raw download with encoded paths, 401/403/404/500 and the rate limit, an
  unreachable server, a client deleted mid-request, the device flow (`authorization_pending`, `slow_down`,
  `access_denied`, `expired_token`, device flow disabled), the cache, settings, and both dialogs. With
  `SAVVYCAN_SCREENSHOTS` set it saves pictures of the dialogs.
- Phase 2 adds: save (201/200), 409 conflict, new branch and pull request, repo creation (201, 422 name taken, 403
  missing permission, organisation owner).
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

- Before testing on real GitHub: register an app with Device Flow enabled (an OAuth App, or a GitHub App with the
  settings in section 4.2) and enter its client ID in File > GitHub Account..., or paste a fine-grained token.
- Create the private scratch repo and add copies of logs and DBC/LDF files from `test/lin-data`.
- For repo creation (phase 2): give the app or token *Administration: read and write*, if wanted.
