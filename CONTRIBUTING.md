# Contributing

## Development workflow

1. Create a feature branch from `main`.
2. Implement the change with unit tests.
3. Run `ctest` locally -- all tests must pass.
4. Open a pull request.

## Coding conventions

- C++17, no exceptions, no RTTI in embedded code.
- Domain modules (`libs/body/`) depend only on port interfaces (`libs/platform/ports/`).
- Every new module includes host-based unit tests using GoogleTest.
- Follow the existing ADR template in `docs/decisions/` for architectural choices.

## Commit messages

Use imperative mood: "Add lighting service" not "Added lighting service".
Keep the first line under 72 characters.

## License

By contributing you agree that your contributions will be licensed under
the project's [Apache-2.0](LICENSE) license.
