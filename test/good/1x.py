[
['input', 'expected', 'description'],
['abcd', 'abcd', 'simple'],
['abcd', 'abcd', 'quoted string in the midst of unquoted string'],
['abcd', 'abcd', 'two quoted strings in the midst of unquoted string'],
['ab\cd', 'ab\cd', 'escape is ignored when not in quotes'],
['ab\\cd', 'ab\\cd', 'escape is ignored when not in quotes'],
['ab\"cd"', 'ab\cd', 'escape is ignored when not in quotes'],
['abcd', 'abcd', 'simple quoted string'],
['ab"cd', 'ab{quote}cd', 'the quote is escaped'],
['ab\cd', 'ab\cd', 'the backslash is escaped'],
['ab\cd', 'ab\cd', 'escape is ignored because c is neither backslash or quote'],
['abcd', 'abcd', 'inquote, out of quote, back in quote'],
['ab-cd', 'ab-cd', 'inquote, out of quote, back in quote'],
]