Logs and DBC Files on GitHub
============================

SavvyCAN can load log files and DBC files straight from GitHub repos, and save them back. The files stay in the
repo, with their history, where others can find them.

- **Load:** File > Load Log File from GitHub..., or Load from GitHub in the DBC File Manager.
- **Save:** File > Save Log File to GitHub..., or Save to GitHub in the DBC File Manager (for the DBC file picked in
  the list).
- **Sign in:** File > GitHub Account..., or the button at the top of the GitHub windows.

New users' repo list starts with Tom-evnut/EV-CANlogs, a public collection of EV CAN logs and DBC files, so there's
something to load straight away.


Before You Start: Signing In and Installing the SavvyCAN App
===========================================================

Loading from **public** repos works without signing in (GitHub then allows 60 requests an hour).

Saving, and loading from **private** repos, need two things:

1. **Sign in.** In File > GitHub Account..., click **Sign in with GitHub**. SavvyCAN copies a code and opens GitHub's
   page: paste the code there and approve SavvyCAN. You only do this once on each computer.
2. **Install the SavvyCAN app on the repos it may use.** Signing in tells GitHub who you are; installing says which
   repos SavvyCAN may read and write. Click **Choose Repos...** in File > GitHub Account... (or go to
   <https://github.com/apps/savvycan/installations/new>), pick your account (or an organisation), and choose:
   - **All repositories** (best for New Repo...), or
   - **Only select repositories**, ticking each repo you'll load from or save to. Add more later the same way.

   GitHub may ask the account's owner to accept new permissions when the SavvyCAN app is updated: accept them on the
   app's installation page (GitHub > Settings > Applications > Installed GitHub Apps > SavvyCAN > Configure).

**Without the install, saving fails** even though you're signed in and the repo shows up. SavvyCAN checks this for
you: the Save to GitHub window says so under the file list as soon as you pick a repo the SavvyCAN app isn't
installed on, and asks before saving there (**Choose Repos...** opens GitHub's page; **Save Anyway** tries anyway).
New Repo... asks the same way when the app isn't installed on all the owner's repos. (Signed in with a token, there's
nothing to check: the token's own repos apply.)

**Other ways to sign in:** a fine-grained personal access token (GitHub > Settings > Developer settings > Personal
access tokens), made for the repos you choose with Contents: read and write (and Pull requests: read and write for
pull requests, Administration: read and write for New Repo...), pasted under "Or use a personal access token". Or,
for forks and organisations, your own GitHub App or OAuth App: tick "Use my own GitHub App or OAuth App" and enter
its client ID (its Device Flow must be turned on).

**Sign Out** forgets the sign-in on this computer. The sign-in is kept in SavvyCAN's settings, scrambled but not
securely encrypted.


Loading a File
==============

1. Pick the **Repo**. **Add...** adds one by name ("owner/repo") or by a github.com link; **Remove** takes it off the
   list (nothing changes on GitHub). The repo list is the same for logs and DBC files.
2. Pick the **Branch**.
3. **File type** filters the list (for logs, "Autodetect File Type" shows everything and works out the format), and
   **Search** finds files and folders by part of their name.
4. Pick the file and click **Open** (or double-click it).

The file is downloaded into SavvyCAN's cache folder and loaded from there. The status bar shows it as
github:owner/repo/path. DBC files loaded from GitHub load again from that copy when SavvyCAN starts. **Clear
Downloaded Files** in File > GitHub Account... empties the cache.

Files over 100 MB, and files stored with Git LFS, can't be loaded yet.


Saving a File
=============

The Save to GitHub window is the same as the load window, with more at the bottom:

- **Repo** and **Branch:** where to save. **New Repo...** creates a repo (see below).
- **File type:** the format the file is saved in (for logs). The file name follows it: "run.csv" becomes "run.asc".
- **Folder:** click a folder in the list, or type one (new folders are made as needed). Empty means the top of the
  repo.
- **File name:** clicking a file in the list picks its folder and name, to replace it.
- **Commit message:** optional. Otherwise it's "Add run.csv from SavvyCAN" (a new file) or "Update run.csv from
  SavvyCAN" (a replaced one), with the file's own name.
- **Save to a new branch and open a pull request:** instead of saving straight to the branch, SavvyCAN makes a new
  branch (a name is suggested), saves there, and opens a pull request into the branch picked, so someone can review
  the change before it's merged. If the save fails, the new branch is deleted again.

Click **Save**. Afterwards a message links to the commit or the pull request on GitHub.

**Saving back a file loaded from GitHub:** the window suggests where it came from. If the file has changed on GitHub
since you loaded it (someone else saved a newer version), SavvyCAN asks what to do:

- **Overwrite** replaces the version on GitHub with yours (theirs stays in the repo's history);
- **Save Under a New Name** suggests a name like "run (2).csv": check it, then click Save again;
- **Cancel** saves nothing.

Saving over a different file that's already there asks first.

**Sizes:** GitHub warns about files over 50 MB, so SavvyCAN asks before saving one, and refuses files over 100 MB.
A more compact log format makes a file smaller.


Creating a Repo
===============

**New Repo...** in the Save to GitHub window creates a repo:

- **Owner:** your account, or one of your organisations;
- **Name** and an optional **Description**;
- **Private:** ticked by default, so only you, and the people you choose, can see it.

The repo starts with a README file, so it has a branch to save into. It's added to your repo list and picked, ready
to save.

New Repo... works best with the SavvyCAN app installed on **All repositories** of that account (see "Before You
Start"). With **Only select repositories**, GitHub may refuse to create the repo, or the new repo may need adding to
the installation (Choose Repos...) before SavvyCAN can save into it. An organisation can also stop its members from
creating repos. You can always create the repo on github.com instead, install the SavvyCAN app on it, and add it with
**Add...**.


When Something Goes Wrong
=========================

- **"Not found on GitHub, or this account has no access to it":** the repo or file isn't there, or it's private and
  you aren't signed in, or the SavvyCAN app isn't installed on it. The message links to the install page when that
  may be the reason.
- **Saving or New Repo... is refused (an error with 400, 403 or 404, or "Resource not accessible by integration"):**
  check the SavvyCAN app is installed on that repo (on All repositories, for New Repo...), and that any new
  permissions have been accepted (see "Before You Start"). The error message says what GitHub objected to.
- **"GitHub didn't accept the sign-in":** the sign-in was withdrawn or has expired: sign in again in File > GitHub
  Account....
- **"GitHub's request limit is used up":** signing in raises the limit a lot.
- **"Couldn't reach GitHub":** check the internet connection (and any proxy).
