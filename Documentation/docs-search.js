(function () {
    function onReady(callback) {
        if (document.readyState === "loading") {
            document.addEventListener("DOMContentLoaded", callback);
            return;
        }

        callback();
    }

    function normalizeText(value) {
        return (value || "").toLowerCase().replace(/\s+/g, " ").trim();
    }

    function hasClass(element, className) {
        return (" " + element.className + " ").indexOf(" " + className + " ") >= 0;
    }

    function addClass(element, className) {
        if (!hasClass(element, className)) {
            element.className = (element.className + " " + className).replace(/^\s+|\s+$/g, "");
        }
    }

    function removeClass(element, className) {
        element.className = (" " + element.className + " ")
            .replace(" " + className + " ", " ")
            .replace(/^\s+|\s+$/g, "");
    }

    function createTextElement(tagName, className, text) {
        var element = document.createElement(tagName);
        element.className = className;
        element.textContent = text;
        return element;
    }

    function escapeHtml(text) {
        return (text || "")
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/\"/g, "&quot;")
            .replace(/'/g, "&#39;");
    }

    function escapeRegExp(text) {
        return text.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
    }

    function createHighlightedElement(className, text, terms) {
        var element = document.createElement("span");
        var uniqueTerms = [];
        var seenTerms = {};

        element.className = className;

        for (var termIndex = 0; termIndex < terms.length; termIndex++) {
            var term = terms[termIndex];
            if (!term || seenTerms[term]) {
                continue;
            }

            seenTerms[term] = true;
            uniqueTerms.push(term);
        }

        uniqueTerms.sort(function (left, right) {
            return right.length - left.length;
        });

        if (!uniqueTerms.length) {
            element.textContent = text;
            return element;
        }

        var matchPattern = new RegExp("(" + uniqueTerms.map(escapeRegExp).join("|") + ")", "gi");
        var exactPattern = new RegExp("^(" + uniqueTerms.map(escapeRegExp).join("|") + ")$", "i");
        var fragments = String(text || "").split(matchPattern);
        var html = "";

        for (var fragmentIndex = 0; fragmentIndex < fragments.length; fragmentIndex++) {
            var fragment = fragments[fragmentIndex];
            if (!fragment) {
                continue;
            }

            if (exactPattern.test(fragment)) {
                html += '<strong class="doc-search-result-highlight">' + escapeHtml(fragment) + "</strong>";
            } else {
                html += escapeHtml(fragment);
            }
        }

        element.innerHTML = html;
        return element;
    }

    onReady(function () {
        var input = document.getElementById("doc-search-input");
        var popup = document.getElementById("doc-search-popup");
        var summary = document.getElementById("doc-search-summary");
        var resultsContainer = document.getElementById("doc-search-results");
        var searchRoot = window.TENDocRoot || "";
        var index = window.TENSearchIndex || [];
        var activeIndex = -1;
        var currentResults = [];

        if (!input || !popup || !summary || !resultsContainer) {
            return;
        }

        for (var itemIndex = 0; itemIndex < index.length; itemIndex++) {
            var entry = index[itemIndex];
            entry.text = (entry.text || "").replace(/\s+/g, " ").trim();
            entry.searchText = normalizeText([
                entry.title,
                entry.pageTitle,
                entry.section,
                entry.text,
                entry.path
            ].join(" "));
        }

        function hidePopup() {
            addClass(popup, "doc-search-popup-hidden");
            activeIndex = -1;
        }

        function showPopup() {
            removeClass(popup, "doc-search-popup-hidden");
        }

        function updateActiveResult() {
            var resultNodes = resultsContainer.getElementsByTagName("a");
            for (var resultIndex = 0; resultIndex < resultNodes.length; resultIndex++) {
                if (resultIndex === activeIndex) {
                    addClass(resultNodes[resultIndex], "doc-search-result-active");
                } else {
                    removeClass(resultNodes[resultIndex], "doc-search-result-active");
                }
            }
        }

        function buildExcerpt(entry, query, terms) {
            var text = entry.text || "";
            var lowerText = text.toLowerCase();
            var matchIndex = query ? lowerText.indexOf(query) : -1;
            var hasMatch = matchIndex >= 0;

            if (matchIndex < 0) {
                for (var termIndex = 0; termIndex < terms.length; termIndex++) {
                    matchIndex = lowerText.indexOf(terms[termIndex]);
                    if (matchIndex >= 0) {
                        hasMatch = true;
                        break;
                    }
                }
            }

            if (matchIndex < 0) {
                matchIndex = 0;
            }

            var start = Math.max(0, matchIndex - 60);
            var end = Math.min(text.length, matchIndex + 160);
            var excerpt = text.slice(start, end).trim();

            if (start > 0) {
                excerpt = "... " + excerpt;
            }

            if (end < text.length) {
                excerpt += " ...";
            }

            return {
                hasMatch: hasMatch,
                text: excerpt
            };
        }

        function search(query) {
            var normalizedQuery = normalizeText(query);
            if (!normalizedQuery) {
                return [];
            }

            var terms = normalizedQuery.split(" ");
            var matches = [];

            for (var searchIndex = 0; searchIndex < index.length; searchIndex++) {
                var candidate = index[searchIndex];
                var isMatch = true;

                for (var termIndex = 0; termIndex < terms.length; termIndex++) {
                    if (candidate.searchText.indexOf(terms[termIndex]) < 0) {
                        isMatch = false;
                        break;
                    }
                }

                if (!isMatch) {
                    continue;
                }

                var score = 0;
                var loweredTitle = normalizeText(candidate.title);
                if (loweredTitle.indexOf(normalizedQuery) === 0) {
                    score += 400;
                } else if (loweredTitle.indexOf(normalizedQuery) >= 0) {
                    score += 250;
                }

                if (candidate.searchText.indexOf(normalizedQuery) >= 0) {
                    score += 100;
                }

                for (var scoreTermIndex = 0; scoreTermIndex < terms.length; scoreTermIndex++) {
                    if (loweredTitle.indexOf(terms[scoreTermIndex]) === 0) {
                        score += 50;
                    } else if (loweredTitle.indexOf(terms[scoreTermIndex]) >= 0) {
                        score += 20;
                    }
                }

                matches.push({
                    entry: candidate,
                    score: score,
                    excerpt: buildExcerpt(candidate, normalizedQuery, terms),
                    terms: normalizedQuery.indexOf(" ") >= 0 ? [normalizedQuery].concat(terms) : terms
                });
            }

            matches.sort(function (left, right) {
                if (right.score !== left.score) {
                    return right.score - left.score;
                }

                return left.entry.title.localeCompare(right.entry.title);
            });

            return matches;
        }

        function renderResults(query) {
            resultsContainer.innerHTML = "";
            currentResults = search(query);
            activeIndex = currentResults.length > 0 ? 0 : -1;

            if (!query) {
                summary.textContent = "Type to search all generated documentation files.";
                hidePopup();
                return;
            }

            showPopup();

            if (currentResults.length === 0) {
                summary.textContent = "No results found.";
                return;
            }

            summary.textContent = currentResults.length + (currentResults.length === 1 ? " result" : " results");

            for (var resultIndex = 0; resultIndex < currentResults.length; resultIndex++) {
                (function (indexInResults) {
                    var result = currentResults[indexInResults];
                    var link = document.createElement("a");
                    link.className = "doc-search-result";
                    link.href = searchRoot + result.entry.path;

                    link.appendChild(createTextElement("span", "doc-search-result-title", result.entry.title));
                    link.appendChild(createTextElement("span", "doc-search-result-meta", result.entry.section + " | " + result.entry.path));
                    if (result.excerpt.hasMatch) {
                        link.appendChild(createHighlightedElement("doc-search-result-excerpt", result.excerpt.text, result.terms));
                    } else {
                        link.appendChild(createTextElement("span", "doc-search-result-excerpt", result.excerpt.text));
                    }

                    link.addEventListener("mouseenter", function () {
                        activeIndex = indexInResults;
                        updateActiveResult();
                    });

                    resultsContainer.appendChild(link);
                }(resultIndex));
            }

            updateActiveResult();
        }

        function openActiveResult() {
            if (activeIndex < 0 || activeIndex >= currentResults.length) {
                return;
            }

            window.location.href = searchRoot + currentResults[activeIndex].entry.path;
        }

        input.addEventListener("input", function () {
            renderResults(input.value);
        });

        input.addEventListener("focus", function () {
            if (input.value) {
                renderResults(input.value);
            }
        });

        input.addEventListener("keydown", function (event) {
            if (event.key === "Escape") {
                hidePopup();
                return;
            }

            if (!currentResults.length) {
                return;
            }

            if (event.key === "ArrowDown") {
                activeIndex = Math.min(currentResults.length - 1, activeIndex + 1);
                updateActiveResult();
                event.preventDefault();
                return;
            }

            if (event.key === "ArrowUp") {
                activeIndex = Math.max(0, activeIndex - 1);
                updateActiveResult();
                event.preventDefault();
                return;
            }

            if (event.key === "Enter") {
                openActiveResult();
                event.preventDefault();
            }
        });

        document.addEventListener("click", function (event) {
            if (!document.getElementById("doc-search").contains(event.target)) {
                hidePopup();
            }
        });

        if (!index.length) {
            summary.textContent = "Search index unavailable.";
        }
    });
}());