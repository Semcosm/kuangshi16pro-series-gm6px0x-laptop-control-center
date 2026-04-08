# CI And Branch Protection

GitHub Actions now defines one workflow, `ci`, with these job names:

- `lint`
- `build`
- `test`
- `integration-smoke`
- `install-smoke`

Recommended required checks for the `main` protected branch:

- `ci / lint`
- `ci / build`
- `ci / test`
- `ci / integration-smoke`
- `ci / install-smoke`

What each check covers:

- `ci / lint`
  project-script shell syntax, optional `shellcheck`, and trailing-whitespace hygiene
- `ci / build`
  clean build of `lccctl` and `lccd`
- `ci / test`
  unit-test binary build and execution
- `ci / integration-smoke`
  mock-backed `lccd` startup, D-Bus introspection, and product-command happy path
- `ci / install-smoke`
  staged install layout, rewritten `lccd.service` path, and uninstall cleanup

Suggested branch protection settings:

1. Enable branch protection for `main`.
2. Require status checks to pass before merging.
3. Add the five checks listed above.
4. Require branches to be up to date before merging.
5. Optionally require pull requests and disallow direct pushes.

Final manual confirmation on GitHub:

1. Open the repository settings page for the `main` branch protection rule.
2. Confirm the required checks appear with the exact names:
   - `ci / lint`
   - `ci / build`
   - `ci / test`
   - `ci / integration-smoke`
   - `ci / install-smoke`
3. Confirm the rule is enabled on `main`, not only drafted in workflow files.
4. Confirm direct pushes are blocked if that is the intended policy.

Repository code can define workflows and document the expected checks, but the
actual protected-branch rule still has to be enabled and verified on GitHub.
