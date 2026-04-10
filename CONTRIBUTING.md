# Contributing to 5G UE Simulation

Thank you for your interest in contributing to the 5G UE Simulation application! This document provides guidelines and procedures for contributing to the project.

## Code of Conduct

All contributors are expected to follow our Code of Conduct, which promotes a respectful and inclusive environment for everyone involved in the project.

## How to Contribute

### Reporting Issues

**Before creating an issue:**
- Check the existing issues to avoid duplicates
- Ensure you're using the latest version of the application
- Read the documentation to confirm it's not expected behavior

**When creating an issue:**
- Use a clear and descriptive title
- Provide steps to reproduce the problem
- Include expected vs. actual behavior
- Specify your environment (OS, compiler version, etc.)
- Include relevant logs, configuration files, or code snippets
- Use appropriate labels (bug, enhancement, question, etc.)

### Suggesting Enhancements

We welcome suggestions for new features and improvements! When suggesting enhancements:

- Clearly describe the proposed feature
- Explain the problem it solves or value it adds
- Provide use cases and examples
- Consider backward compatibility
- Discuss potential implementation approaches

### Code Contributions

#### Getting Started

1. Fork the repository
2. Create a feature branch from `main`
3. Make your changes
4. Add tests if applicable
5. Update documentation
6. Submit a pull request

#### Development Setup

```bash
# Clone your fork
git clone https://github.com/your-username/uesim.git
cd uesim

# Create a feature branch
git checkout -b feature/your-feature-name

# Build and test
make
make test
```

#### Coding Standards

Please follow the guidelines in our [Development Guide](docs/development.md):

- **C Language Standard**: C11 with GNU extensions
- **Naming Conventions**: 
  - Functions: `snake_case` with module prefix
  - Variables: `snake_case` with descriptive names
  - Constants: `UPPER_SNAKE_CASE`
  - Types: `snake_case_t`
- **Memory Management**: Use custom allocation functions
- **Error Handling**: Check all return values, propagate errors properly
- **Thread Safety**: Use appropriate synchronization primitives
- **Documentation**: Document public APIs and complex logic

#### Code Review Process

All contributions must pass code review. Reviewers will check:

- Code correctness and efficiency
- Adherence to coding standards
- Proper error handling
- Thread safety
- Memory management
- Documentation quality
- Test coverage

#### Pull Request Guidelines

**Before submitting:**
- Ensure your code compiles without warnings
- Run all tests and ensure they pass
- Update documentation for any API changes
- Follow the commit message format
- Keep changes focused and atomic

**Pull Request Template:**
```markdown
## Description
Brief description of the changes

## Related Issue
Fixes #123

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change
- [ ] Documentation update

## Testing
- [ ] Unit tests added/updated
- [ ] Integration tests added/updated
- [ ] Manual testing performed

## Checklist
- [ ] Code follows project coding standards
- [ ] Documentation updated
- [ ] Tests pass
- [ ] No conflicts with main branch
```

## Development Workflow

### Branch Naming Convention

- `feature/` - New features
- `bugfix/` - Bug fixes
- `hotfix/` - Critical production fixes
- `release/` - Release preparation
- `docs/` - Documentation updates
- `test/` - Test-related changes

### Commit Message Format

Follow conventional commit format:

```
type(scope): brief description

Detailed description of the changes, why they were made,
and their impact. Reference any related issues.

Resolves: #123
See also: #456
```

**Types:**
- `feat` - New feature
- `fix` - Bug fix
- `docs` - Documentation
- `style` - Code style changes
- `refactor` - Code refactoring
- `test` - Test changes
- `chore` - Maintenance tasks

**Scopes:**
- `core` - Core framework
- `rrc` - RRC layer
- `protocol` - Protocol stack
- `socket` - Socket transport
- `cli` - Command line interface
- `memory` - Memory management
- `thread` - Threading system
- `build` - Build system
- `test` - Testing infrastructure

### Example Commits

```
feat(rrc): implement handover procedure

- Add RRC handover preparation message handling
- Implement handover command processing
- Add handover confirmation procedure
- Update state machine for handover scenarios

Resolves: #123
```

```
fix(socket): resolve SCTP connection timeout issue

- Increase connection timeout from 5s to 30s
- Add proper error handling for connection failures
- Implement connection retry mechanism
- Update logging for connection events

Fixes: #456
```

## Testing

### Test Categories

1. **Unit Tests**: Individual function and module testing
2. **Integration Tests**: Component interaction testing
3. **System Tests**: End-to-end scenario testing
4. **Performance Tests**: Load and stress testing
5. **Regression Tests**: Ensuring existing functionality isn't broken

### Writing Tests

```c
// test_example.c
#include "uesim.h"
#include <assert.h>
#include <stdio.h>

static void test_function_name(void) {
    // Setup
    ue_context_t* ue_ctx = NULL;
    uesim_error_t result;
    
    // Test
    result = uesim_create_ue_instance(&ue_ctx);
    assert(result == UESIM_SUCCESS);
    assert(ue_ctx != NULL);
    
    // Cleanup
    uesim_free(ue_ctx);
    
    printf("✓ test_function_name passed\n");
}

int main(void) {
    test_function_name();
    // Add more tests...
    
    printf("All tests passed!\n");
    return 0;
}
```

### Running Tests

```bash
# Run all tests
make test

# Run specific test suite
make test-unit
make test-integration
make test-performance

# Run with Valgrind for memory checking
make test-valgrind

# Run with AddressSanitizer
make test-asan
```

## Documentation

### Documentation Updates

When contributing code changes, update relevant documentation:

- **API Changes**: Update `docs/api.md`
- **Architecture Changes**: Update `docs/architecture.md`
- **New Features**: Add sections to appropriate documentation
- **Configuration Changes**: Update `etc/uesim.conf` and `README.md`

### Documentation Style

- Use clear, concise language
- Provide examples for complex concepts
- Keep documentation up-to-date with code changes
- Use consistent formatting and structure

## Code Quality

### Static Analysis

The project uses several static analysis tools:

```bash
# Run clang static analyzer
make analyze-clang

# Run cppcheck
make analyze-cppcheck

# Check code formatting
make check-format
```

### Performance Considerations

- Profile performance-critical code
- Avoid unnecessary allocations
- Use efficient data structures
- Consider cache locality
- Minimize lock contention

### Security Review

- Validate all inputs
- Check for buffer overflows
- Review memory management
- Ensure proper error handling
- Follow secure coding practices

## Community

### Communication Channels

- **GitHub Issues**: For bug reports and feature requests
- **GitHub Discussions**: For general discussion and questions
- **Email**: For private inquiries (see MAINTAINERS file)

### Getting Help

If you need help with your contribution:

1. Check the documentation
2. Search existing issues and discussions
3. Ask in GitHub Discussions
4. Contact maintainers directly for complex issues

## Recognition

Contributors will be recognized in:

- Git commit history
- GitHub contributors list
- Project documentation
- Release notes for significant contributions

## License

By contributing to this project, you agree that your contributions will be licensed under the MIT License.

## Questions?

If you have any questions about contributing, please:

1. Open an issue with the "question" label
2. Start a discussion in GitHub Discussions
3. Contact the maintainers directly

Thank you for contributing to the 5G UE Simulation project!