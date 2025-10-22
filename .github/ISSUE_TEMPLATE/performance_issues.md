---
name: ⚡ Performance Issue
about: Report performance problems, slowness, or resource usage issues
title: "[PERFORMANCE] "
labels: "type:performance, status:triage"
assignees: ""
---

## Preliminary Checks
<!-- Please confirm you've done the following by checking the boxes -->
- [ ] I have searched existing issues for similar performance problems
- [ ] I have verified this isn't caused by my system/network
- [ ] I have tried with minimal configuration/plugins

## Performance Issue Details

### Performance Issue Type
<!-- Select the type of performance issue -->
- [ ] Slow startup/initialization
- [ ] Slow data processing
- [ ] High memory usage
- [ ] High CPU usage
- [ ] Slow UI responsiveness
- [ ] Network/API delays
- [ ] Database query performance
- [ ] Memory leaks
- [ ] Other: ___________

### Impact Severity
<!-- How much does this impact your usage? -->
- [ ] Low - Slightly noticeable, doesn't affect workflow
- [ ] Medium - Noticeable delays, minor workflow impact
- [ ] High - Significant delays, major workflow impact
- [ ] Critical - Unusable, blocks all work

## ⚡ Performance Issue Description
<!-- Describe the performance problem in detail -->

**What specific operation or feature is slow?**


**How long does it take vs. how long should it take?**


## 📊 Performance Metrics
<!-- Provide specific performance measurements -->

| Metric | Current Performance | Expected Performance | Difference |
|--------|-------------------|---------------------|------------|
| **Operation Time** | ___ seconds/minutes | ___ seconds/minutes | ___% slower |
| **Memory Usage** | ___ MB/GB | ___ MB/GB | ___% higher |
| **CPU Usage** | ___%  | ___%  | ___% higher |
| **Data Size** | ___ records/MB | ___ records/MB | - |

**Frequency:** How often does this happen?
- [ ] Every time
- [ ] Most of the time (75-99%)
- [ ] Sometimes (25-74%)
- [ ] Rarely (1-24%)

## 🔄 Steps to Reproduce
<!-- Exact steps to reproduce the performance issue -->
1. Load dataset of size X
2. Perform operation Y
3. Observe slow performance
4. Measure timing/resource usage

## 🔍 Profiling Information
<!-- Any profiling data, benchmarks, or performance logs -->

**Tools used for measurement:**
- [ ] Built-in timing
- [ ] cProfile
- [ ] memory_profiler
- [ ] htop/top
- [ ] Browser DevTools
- [ ] Other: ___________

**Profiling Data:**
```
Paste profiling output, performance logs, or benchmark results here
```

**Screenshots of system monitors:**
<!-- Drag and drop images here -->

## 🖥️ Environment Information
<!-- Detailed system information -->

**System Specifications:**
- **OS:** [e.g. Ubuntu 20.04]
- **CPU:** [e.g. Intel i7-9700K, 8 cores]
- **RAM:** [e.g. 16GB]
- **Storage:** [e.g. SSD, HDD]
- **Python Version:** [e.g. 3.11.0]
- **Fincept Terminal Version:** [e.g. 1.2.3]

**Configuration:**
- **Docker:** [ ] Yes [ ] No (Version: _______)
- **Data source:** [e.g. local files, remote API, database]
- **Dataset size:** [e.g. 10k rows, 100MB file]
- **Concurrent users:** [e.g. 1, 10, 100]

## 📈 Performance Comparison
<!-- How does performance compare in different scenarios? -->

**Different data sizes:**
- Small dataset (___): ___
- Medium dataset (___): ___
- Large dataset (___): ___

**Different environments:**
- Local: ___
- Docker: ___
- Cloud: ___

**Previous versions:**
- Version X.X.X: ___
- Current version: ___

**Similar tools comparison:**
- Tool Y: ___
- Tool Z: ___

## 📚 Additional Context
<!-- Any other relevant information -->

**When did you first notice this performance issue?**


**Has performance degraded over time?**


**Any recent changes that might have caused this?**


**Network conditions (if relevant):**


**Other applications running:**


## 🛠️ Potential Solutions
<!-- Any ideas for improving performance -->

**Suggestions:**
- [ ] Caching improvements
- [ ] Algorithm optimization
- [ ] Database indexing
- [ ] Parallel processing
- [ ] Memory optimization
- [ ] Other: ___________

## 🧪 Testing Availability
<!-- Can you help test performance improvements? -->
- [ ] Yes, I can run benchmarks and provide feedback
- [ ] Yes, but need guidance on testing
- [ ] Limited availability for testing
- [ ] No, not available for testing

**Testing commitment:**
- [ ] Can test within 24 hours
- [ ] Can test within a week
- [ ] Can test when convenient
- [ ] Cannot commit to testing timeline

---

**Thank you for helping us improve Fincept Terminal's performance! ⚡**