# SavvyCAN GitHub repos: loading and saving logs and DBCs

Scope: opening and saving log files and DBC/LDF files stored in GitHub repos, from inside SavvyCAN (Tom-evnut fork).
Written against `fork-ci` (2026-10-25). Each phase branches from and merges into `fork-ci`.

---

## 1. Decisions

| Question | Decision |
|---|---|
| Sign-in | Both from the first version: "Sign in with GitHub" (OAuth device flow) and a pasted personal access token |
| Hosts | github.com only. The API base URL is a hidden setting so GitHub Enterprise can come later, untested |
| Log sizes | Mostly under 50 MB: the Contents API is enough; the Git Data API and LFS wait for phase 4 |
| Committing | Straight to the selected branch by default; "new branch and open a pull request" is an option when saving |
| Test repo | A new private scratch repo (e.g. `tomdebree/savvycan-github-test`) with copies of `test/lin-data` files |

---

## 2. How it fits the current code

Every log loader and saver in `FrameFileIO` takes a file path (`autoDetectLoadFile(QString, ...)`,
`saveNativeCSVFile(QString, ...)` and so on), and so do `DBCFile::loadFile` and `DBCFile::saveFile`. Only the
`QFileDialog` picks the path. So:

- **Load:** download the file into a local cache folder, then run the existing loader on it.
- **Save:** write the file into the cache with the existing saver, then upload it as a commit.

The log formats and the DBC/LDF parser don't change. Qt Network is already linked (`SavvyCAN.pro`,
`CMakeLists.txt`), so there's no new library and no need for git or libgit2 on the user's machine.

---

## 3. Design

### 3.1 `GitHubClient` (`github/githubclient.{h,cpp}`)

A wrapper around `QNetworkAccessManager` and GitHub's REST API (`https://api.github.com`):

| Use | Request |
|---|---|
| Branches | `GET /repos/{owner}/{repo}/branches` |
| File tree | `GET /repos/{owner}/{repo}/git/trees/{branch}?recursive=1` (whole tree in one request) |
| Download | `GET /repos/{owner}/{repo}/contents/{path}?ref={branch}` with `Accept: application/vnd.github.raw`; the blob SHA comes from the tree |
| Save | `PUT /repos/{owner}/{repo}/contents/{path}`: base64 content, commit message, branch, and the SHA of the version it replaces |
| New branch | `POST /repos/{owner}/{repo}/git/refs` from the selected branch's head |
| Pull request | `POST /repos/{owner}/{repo}/pulls` |

- A save whose SHA no longer matches gets **409 Conflict**: the user can overwrite, save under a new name, or cancel.
- Errors shown in plain words: 401 (sign in again), 403 with `X-RateLimit-Remaining: 0` (rate limit, with the reset
  time), 404 (no access or no such file), 422 (file too large or bad path).
- Public repos work without signing in, at GitHub's 60 requests an hour.
- Uses the system proxy (`QNetworkProxyFactory::setUseSystemConfiguration(true)`).

### 3.2 Sign-in

Settings get a **GitHub** section with two ways to sign in. `GitHubClient` only sees a token and sends it as
`Authorization: Bearer`.

- **Sign in with GitHub (device flow):** SavvyCAN asks `https://github.com/login/device/code` for a code, shows it
  with a button opening `https://github.com/login/device`, and polls `https://github.com/login/oauth/access_token`
  at the interval GitHub gives until the user approves. Needs an **OAuth App** with "Enable Device Flow" ticked; only
  its client ID is built into SavvyCAN (no client secret). Scope `repo`, which reaches all of the user's private
  repos, or `public_repo` when the user only wants public ones.
- **Personal access token:** pasted in, for users who want access limited to chosen repos (a fine-grained token with
  *Contents: read and write* and, for pull requests, *Pull requests: read and write*). The settings text recommends
  this for tighter access.
- Either token is stored with `SimpleCrypt`, the same way as the remote password in `MainSettingsDialog`. That only
  hides it from a casual look, and the settings text says so. "Sign out" deletes it.
- The signed-in user's name (`GET /user`) is shown so it's clear which account is in use.

### 3.3 Saved repos

A list in `QSettings` (`GitHub/Repos`): owner/repo, default branch, what it holds (logs, DBCs or both) and a default
folder. Added from the browser dialog or settings, from `owner/repo` or a pasted github.com URL.

### 3.4 Local cache

- Files go to `QStandardPaths::CacheLocation/github/<owner>/<repo>/<branch>/<path>`, keeping the real file name so
  autodetect still sees the extension.
- A `.origin.json` next to each file records repo, branch, path and blob SHA; saving back reads it.
- Settings get "Clear GitHub cache" and a size limit (oldest files go first).

### 3.5 `GitHubBrowserDialog`

One dialog for logs and DBCs:

- Repo and branch pickers, a file tree filtered by extension (log extensions, or `.dbc`/`.ldf`), a search box.
- **Open mode:** pick a file.
- **Save mode:** pick a folder, file name, format (logs), commit message, and a "Commit to a new branch and open a
  pull request" checkbox with branch name and PR title. After saving, the commit or PR link is shown.
- Downloads and uploads show a progress dialog that can be cancelled; the requests run asynchronously behind it.

### 3.6 Refactor first (phase 0)

- `FrameFileIO::loadFrameFile` and `saveFrameFile` mix the file dialog with choosing the format. Split them into
  `loadFrameFileAs(path, filterIndex, frames)` and `saveFrameFileAs(path, filterIndex, frames)`, plus shared
  functions returning the filter lists, so the GitHub path reuses the same formats and the LIN-format warning.
- `DBCHandler::loadDBCFile(int)` and `saveDBCFile(int)` get the same split.

---

## 4. Logs

- **File menu:** "Open Log from GitHub..." and "Save Log to GitHub...", next to Open Log File and Save Log File.
- **Open:** pick a file, download it, run `autoDetectLoadFile`, then the same steps as `handleDroppedFile`
  (including "salvage what could be loaded?"). The file status shows `github:owner/repo/path`.
- **Save:** the chosen saver writes into the cache, then it's uploaded. A log that came from GitHub suggests its own
  path and passes its SHA, so the conflict check works.
- **Size:** above about 50 MB the save dialog warns (GitHub warns at 50 MB and refuses 100 MB without LFS) and
  suggests BLF or pcapng, which are much smaller than CSV.

---

## 5. DBCs

- **DBC window (`DBCLoadSaveWindow`):** "Load from GitHub" and "Save to GitHub" next to Load and Save.
- **Load:** download, then `DBCHandler::loadDBCFile(path)`. The `DBCFile` remembers its GitHub origin and SHA.
- **Save:** a DBC from GitHub commits back to its own path after asking for a commit message, with the conflict
  check. LDF files are saved as `.dbc`, as the local save does.
- **Startup (phase 3):** `DBC/Origin_N` is saved next to `DBC/Filename_N`. At startup the cached copy loads at once;
  a quiet check against GitHub then offers to reload if the file changed there. Nothing is replaced without asking.
- **Later, optional:** load every `.dbc` in a repo folder, with buses assigned from a small `savvycan.json` in the
  repo.

---

## 6. Phases

| Phase | Content | Branch |
|---|---|---|
| 0 | Split the file dialogs from the load/save code in `FrameFileIO` and `DBCHandler`, no behaviour change; unit tests | `github-phase0-refactor` |
| 1 | `GitHubClient` read side, device flow and token sign-in, cache, browser dialog; **load** logs and DBCs | `github-phase1-load` |
| 2 | **Save** logs and DBCs through the Contents API, 409 conflict handling, new branch and pull request option | `github-phase2-save` |
| 3 | DBC GitHub origin restored at startup, upstream change check, recent GitHub files | `github-phase3-dbc-sync` |
| 4 | Git Data API for large files, LFS, GitHub App sign-in (per-repo access), OS keychain storage, Enterprise | later |

---

## 7. Testing

- `unit_tests/test_githubclient.cpp` against a local `QTcpServer` faking GitHub: tree parsing, raw download, device
  flow (`authorization_pending`, `slow_down`, `expired_token`, `access_denied`), 401/403/404/422, rate limit, 409
  conflict, new branch and pull request. Runs in the `savvycan-test:qt6.10` Docker image.
- Round trip against the scratch repo: load a log and a DBC, save them back, compare byte for byte; edit the file on
  github.com in between to check the conflict prompt.
- Save, conflict and pull request tests only ever touch the scratch repo.

---

## 8. Risks

- **TLS in packaged builds:** `build.yml` and `fork-build.yml` don't mention TLS. Qt 6 on Windows needs the
  `tls/qschannelbackend` plugin deployed; Linux builds need OpenSSL 3 at run time. Without them every request fails,
  so phase 1 starts by checking the CI artifacts.
- **Token storage:** `SimpleCrypt` isn't real protection; phase 4 moves tokens to the OS keychain.
- **Broad device-flow scope:** an OAuth App token with `repo` reaches every private repo of the user. A GitHub App
  (phase 4) would limit it to the repos it's installed on.

---

## 9. Needed from the user before phase 1 testing

- Register an OAuth App (GitHub Settings > Developer settings > OAuth Apps), tick "Enable Device Flow", and give its
  client ID.
- Create the private scratch repo and add copies of logs and DBC/LDF files from `test/lin-data`.
