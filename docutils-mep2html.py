#!/usr/bin/env python
"""Convert MEPs to (X)HTML - courtesy of /F

Usage: %(PROGRAM)s [options] [<meps> ...]

Options:

-u, --user
    python.org username

-b, --browse
    After generating the HTML, direct your web browser to view it
    (using the Python webbrowser module).  If both -i and -b are
    given, this will browse the on-line HTML; otherwise it will
    browse the local HTML.  If no pep arguments are given, this
    will browse PEP 0.

-i, --install
    After generating the HTML, install it and the plaintext source file
    (.txt) on python.org.  In that case the user's name is used in the scp
    and ssh commands, unless "-u username" is given (in which case, it is
    used instead).  Without -i, -u is ignored.

-l, --local
    Same as -i/--install, except install on the local machine.  Use this
    when logged in to the python.org machine (creosote).

-q, --quiet
    Turn off verbose messages.

-h, --help
    Print this help message and exit.

The optional arguments ``peps`` are either pep numbers or .txt files.
"""

import sys
import os
import re
import cgi
import glob
import getopt
import errno
import random
import time

REQUIRES = {'python': '2.2',
            'docutils': '0.2.7'}
PROGRAM = sys.argv[0]
RFCURL = 'http://www.faqs.org/rfcs/rfc%d.html'
MEPURL = 'mep-%04d.html'
MEPCVSURL = ('http://cvs.sourceforge.net/cgi-bin/viewcvs.cgi/python/python'
             '/nondist/meps/mep-%04d.txt')
MEPDIRRUL = 'http://www.python.org/meps/'


HOST = "www.python.org"                    # host for update
HDIR = "/ftp/ftp.python.org/pub/www.python.org/meps" # target host directory
LOCALVARS = "Local Variables:"

COMMENT = """<!--
This HTML is auto-generated.  DO NOT EDIT THIS FILE!  If you are writing a new
MEP, see http://www.python.org/meps/mep-0001.html for instructions and links
to templates.  DO NOT USE THIS HTML FILE AS YOUR TEMPLATE!
-->"""

# The generated HTML doesn't validate -- you cannot use <hr> and <h3> inside
# <pre> tags.  But if I change that, the result doesn't look very nice...
DTD = ('<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN"\n'
       '                      "http://www.w3.org/TR/REC-html40/loose.dtd">')

fixpat = re.compile("((http|ftp):[-_a-zA-Z0-9/.+~:?#$=&,]+)|(mep-\d+(.txt)?)|"
                    "(RFC[- ]?(?P<rfcnum>\d+))|"
                    "(MEP\s+(?P<mepnum>\d+))|"
                    ".")

EMPTYSTRING = ''
SPACE = ' '
COMMASPACE = ', '



def usage(code, msg=''):
    """Print usage message and exit.  Uses stderr if code != 0."""
    if code == 0:
        out = sys.stdout
    else:
        out = sys.stderr
    print >> out, __doc__ % globals()
    if msg:
        print >> out, msg
    sys.exit(code)



def fixanchor(current, match):
    text = match.group(0)
    link = None
    if text.startswith('http:') or text.startswith('ftp:'):
        # Strip off trailing punctuation.  Pattern taken from faqwiz.
        ltext = list(text)
        while ltext:
            c = ltext.pop()
            if c not in '();:,.?\'"<>':
                ltext.append(c)
                break
        link = EMPTYSTRING.join(ltext)
    elif text.startswith('mep-') and text <> current:
        link = os.path.splitext(text)[0] + ".html"
    elif text.startswith('MEP'):
        mepnum = int(match.group('mepnum'))
        link = MEPURL % mepnum
    elif text.startswith('RFC'):
        rfcnum = int(match.group('rfcnum'))
        link = RFCURL % rfcnum
    if link:
        return '<a href="%s">%s</a>' % (cgi.escape(link), cgi.escape(text))
    return cgi.escape(match.group(0)) # really slow, but it works...



NON_MASKED_EMAILS = [
    'meps@python.org',
    'python-list@python.org',
    'python-dev@python.org',
    ]

def fixemail(address, mepno):
    if address.lower() in NON_MASKED_EMAILS:
        # return hyperlinked version of email address
        return linkemail(address, mepno)
    else:
        # return masked version of email address
        parts = address.split('@', 1)
        return '%s&#32;&#97;t&#32;%s' % (parts[0], parts[1])


def linkemail(address, mepno):
    parts = address.split('@', 1)
    return ('<a href="mailto:%s&#64;%s?subject=MEP%%20%s">'
            '%s&#32;&#97;t&#32;%s</a>'
            % (parts[0], parts[1], mepno, parts[0], parts[1]))


def fixfile(inpath, input_lines, outfile):
    from email.Utils import parseaddr
    basename = os.path.basename(inpath)
    infile = iter(input_lines)
    # convert plaintext mep to minimal XHTML markup
    print >> outfile, DTD
    print >> outfile, '<html>'
    print >> outfile, COMMENT
    print >> outfile, '<head>'
    # head
    header = []
    mep = ""
    title = ""
    for line in infile:
        if not line.strip():
            break
        if line[0].strip():
            if ":" not in line:
                break
            key, value = line.split(":", 1)
            value = value.strip()
            header.append((key, value))
        else:
            # continuation line
            key, value = header[-1]
            value = value + line
            header[-1] = key, value
        if key.lower() == "title":
            title = value
        elif key.lower() == "mep":
            mep = value
    if mep:
        title = "MEP " + mep + " -- " + title
    if title:
        print >> outfile, '  <title>%s</title>' % cgi.escape(title)
    #r = random.choice(range(64))
    print >> outfile, (
        '  <link rel="STYLESHEET" href="style.css" type="text/css" />\n'
        '</head>\n'
        '<body bgcolor="white">\n'
        '<table class="navigation" cellpadding="0" cellspacing="0"\n'
        '       width="100%%" border="0">\n'
        '<tr><td class="navicon" width="107" height="47">\n'
        '<a href="http://www.mozart-oz.org/" title="Mozart/Oz Home Page">\n'
        '<img src="mozart-logo.gif" alt="[Mozart/Oz]"\n'
        ' border="0" width="107" height="47" /></a></td>\n'
        '<td class="textlinks" align="left">\n'
        '[<b><a href="http://www.mozart-oz.org/">Mozart/Oz Home</a></b>]')
    if basename <> 'mep-0000.txt':
        print >> outfile, '[<b><a href="mep-0000.html">MEP Index</a></b>]'
    if mep:
        try:
            print >> outfile, ('[<b><a href="mep-%04d.txt">MEP Source</a>'
                               '</b>]' % int(mep))
        except ValueError, error:
            print >> sys.stderr, ('ValueError (invalid MEP number): %s'
                                  % error)
    print >> outfile, '</td></tr></table>'
    print >> outfile, '<div class="header">\n<table border="0">'
    for k, v in header:
        if k.lower() in ('author', 'discussions-to'):
            mailtos = []
            for part in re.split(',\s*', v):
                if '@' in part:
                    realname, addr = parseaddr(part)
                    if k.lower() == 'discussions-to':
                        m = linkemail(addr, mep)
                    else:
                        m = fixemail(addr, mep)
                    mailtos.append('%s &lt;%s&gt;' % (realname, m))
                elif part.startswith('http:'):
                    mailtos.append(
                        '<a href="%s">%s</a>' % (part, part))
                else:
                    mailtos.append(part)
            v = COMMASPACE.join(mailtos)
        elif k.lower() in ('replaces', 'replaced-by', 'requires'):
            othermeps = ''
            for othermep in re.split(',?\s+', v):
                othermep = int(othermep)
                othermeps += '<a href="mep-%04d.html">%i</a> ' % (othermep,
                                                                  othermep)
            v = othermeps
        elif k.lower() in ('last-modified',):
            date = v or time.strftime('%d-%b-%Y',
                                      time.localtime(os.stat(inpath)[8]))
            try:
                url = MEPCVSURL % int(mep)
                v = '<a href="%s">%s</a> ' % (url, cgi.escape(date))
            except ValueError, error:
                v = date
        elif k.lower() in ('content-type',):
            url = MEPURL % 9
            mep_type = v or 'text/plain'
            v = '<a href="%s">%s</a> ' % (url, cgi.escape(mep_type))
        else:
            v = cgi.escape(v)
        print >> outfile, '  <tr><th>%s:&nbsp;</th><td>%s</td></tr>' \
              % (cgi.escape(k), v)
    print >> outfile, '</table>'
    print >> outfile, '</div>'
    print >> outfile, '<hr />'
    print >> outfile, '<div class="content">'
    need_pre = 1
    for line in infile:
        if line[0] == '\f':
            continue
        if line.strip() == LOCALVARS:
            break
        if line[0].strip():
            if not need_pre:
                print >> outfile, '</pre>'
            print >> outfile, '<h3>%s</h3>' % line.strip()
            need_pre = 1
        elif not line.strip() and need_pre:
            continue
        else:
            # MEP 0 has some special treatment
            if basename == 'mep-0000.txt':
                parts = line.split()
                if len(parts) > 1 and re.match(r'\s*\d{1,4}', parts[1]):
                    # This is a MEP summary line, which we need to hyperlink
                    url = MEPURL % int(parts[1])
                    if need_pre:
                        print >> outfile, '<pre>'
                        need_pre = 0
                    print >> outfile, re.sub(
                        parts[1],
                        '<a href="%s">%s</a>' % (url, parts[1]),
                        line, 1),
                    continue
                elif parts and '@' in parts[-1]:
                    # This is a mep email address line, so filter it.
                    url = fixemail(parts[-1], mep)
                    if need_pre:
                        print >> outfile, '<pre>'
                        need_pre = 0
                    print >> outfile, re.sub(
                        parts[-1], url, line, 1),
                    continue
            line = fixpat.sub(lambda x, c=inpath: fixanchor(c, x), line)
            if need_pre:
                print >> outfile, '<pre>'
                need_pre = 0
            outfile.write(line)
    if not need_pre:
        print >> outfile, '</pre>'
    print >> outfile, '</div>'
    print >> outfile, '</body>'
    print >> outfile, '</html>'


docutils_settings = None
"""Runtime settings object used by Docutils.  Can be set by the client
application when this module is imported."""

def fix_rst_mep(inpath, input_lines, outfile):
    from docutils import core
    output = core.publish_string(
        source=''.join(input_lines),
        source_path=inpath,
        destination_path=outfile.name,
        reader_name='mep',
        parser_name='restructuredtext',
        writer_name='mep_html',
        settings=docutils_settings,
        # Allow Docutils traceback if there's an exception:
        settings_overrides={'traceback': 1})
    outfile.write(output)


def get_mep_type(input_lines):
    """
    Return the Content-Type of the input.  "text/plain" is the default.
    Return ``None`` if the input is not a MEP.
    """
    mep_type = None
    for line in input_lines:
        line = line.rstrip().lower()
        if not line:
            # End of the RFC 2822 header (first blank line).
            break
        elif line.startswith('content-type: '):
            mep_type = line.split()[1] or 'text/plain'
            break
        elif line.startswith('mep: '):
            # Default MEP type, used if no explicit content-type specified:
            mep_type = 'text/plain'
    return mep_type


def get_input_lines(inpath):
    try:
        infile = open(inpath)
    except IOError, e:
        if e.errno <> errno.ENOENT: raise
        print >> sys.stderr, 'Error: Skipping missing MEP file:', e.filename
        sys.stderr.flush()
        return None, None
    lines = infile.read().splitlines(1) # handles x-platform line endings
    infile.close()
    return lines


def find_mep(mep_str):
    """Find the .txt file indicated by a cmd line argument"""
    if os.path.exists(mep_str):
        return mep_str
    num = int(mep_str)
    return "mep-%04d.txt" % num

def make_html(inpath, verbose=0):
    input_lines = get_input_lines(inpath)
    mep_type = get_mep_type(input_lines)
    if mep_type is None:
        print >> sys.stderr, 'Error: Input file %s is not a MEP.' % inpath
        sys.stdout.flush()
        return None
    elif not MEP_TYPE_DISPATCH.has_key(mep_type):
        print >> sys.stderr, ('Error: Unknown MEP type for input file %s: %s'
                              % (inpath, mep_type))
        sys.stdout.flush()
        return None
    elif MEP_TYPE_DISPATCH[mep_type] == None:
        mep_type_error(inpath, mep_type)
        return None
    outpath = os.path.splitext(inpath)[0] + ".html"
    if verbose:
        print inpath, "(%s)" % mep_type, "->", outpath
        sys.stdout.flush()
    outfile = open(outpath, "w")
    MEP_TYPE_DISPATCH[mep_type](inpath, input_lines, outfile)
    outfile.close()
    os.chmod(outfile.name, 0664)
    return outpath

def push_mep(htmlfiles, txtfiles, username, verbose, local=0):
    quiet = ""
    if local:
        if verbose:
            quiet = "-v"
        target = HDIR
        copy_cmd = "cp"
        chmod_cmd = "chmod"
    else:
        if not verbose:
            quiet = "-q"
        if username:
            username = username + "@"
        target = username + HOST + ":" + HDIR
        copy_cmd = "scp"
        chmod_cmd = "ssh %s%s chmod" % (username, HOST)
    files = htmlfiles[:]
    files.extend(txtfiles)
    files.append("style.css")
    files.append("mep.css")
    filelist = SPACE.join(files)
    rc = os.system("%s %s %s %s" % (copy_cmd, quiet, filelist, target))
    if rc:
        sys.exit(rc)
    rc = os.system("%s 664 %s/*" % (chmod_cmd, HDIR))
    if rc:
        sys.exit(rc)


MEP_TYPE_DISPATCH = {'text/plain': fixfile,
                     'text/x-rst': fix_rst_mep}
MEP_TYPE_MESSAGES = {}

def check_requirements():
    # Check Python:
    try:
        from email.Utils import parseaddr
    except ImportError:
        MEP_TYPE_DISPATCH['text/plain'] = None
        MEP_TYPE_MESSAGES['text/plain'] = (
            'Python %s or better required for "%%(mep_type)s" MEP '
            'processing; %s present (%%(inpath)s).'
            % (REQUIRES['python'], sys.version.split()[0]))
    # Check Docutils:
    try:
        import docutils
    except ImportError:
        MEP_TYPE_DISPATCH['text/x-rst'] = None
        MEP_TYPE_MESSAGES['text/x-rst'] = (
            'Docutils not present for "%(mep_type)s" MEP file %(inpath)s.  '
            'See README.txt for installation.')
    else:
        installed = [int(part) for part in docutils.__version__.split('.')]
        required = [int(part) for part in REQUIRES['docutils'].split('.')]
        if installed < required:
            MEP_TYPE_DISPATCH['text/x-rst'] = None
            MEP_TYPE_MESSAGES['text/x-rst'] = (
                'Docutils must be reinstalled for "%%(mep_type)s" MEP '
                'processing (%%(inpath)s).  Version %s or better required; '
                '%s present.  See README.txt for installation.'
                % (REQUIRES['docutils'], docutils.__version__))

def mep_type_error(inpath, mep_type):
    print >> sys.stderr, 'Error: ' + MEP_TYPE_MESSAGES[mep_type] % locals()
    sys.stdout.flush()


def browse_file(mep):
    import webbrowser
    file = find_mep(mep)
    if file.endswith(".txt"):
        file = file[:-3] + "html"
    file = os.path.abspath(file)
    url = "file:" + file
    webbrowser.open(url)

def browse_remote(mep):
    import webbrowser
    file = find_mep(mep)
    if file.endswith(".txt"):
        file = file[:-3] + "html"
    url = MEPDIRRUL + file
    webbrowser.open(url)


def main(argv=None):
    # defaults
    update = 0
    local = 0
    username = ''
    verbose = 1
    browse = 0

    check_requirements()

    if argv is None:
        argv = sys.argv[1:]

    try:
        opts, args = getopt.getopt(
            argv, 'bilhqu:',
            ['browse', 'install', 'local', 'help', 'quiet', 'user='])
    except getopt.error, msg:
        usage(1, msg)

    for opt, arg in opts:
        if opt in ('-h', '--help'):
            usage(0)
        elif opt in ('-i', '--install'):
            update = 1
        elif opt in ('-l', '--local'):
            update = 1
            local = 1
        elif opt in ('-u', '--user'):
            username = arg
        elif opt in ('-q', '--quiet'):
            verbose = 0
        elif opt in ('-b', '--browse'):
            browse = 1

    if args:
        meptxt = []
        html = []
        for mep in args:
            file = find_mep(mep)
            meptxt.append(file)
            newfile = make_html(file, verbose=verbose)
            if newfile:
                html.append(newfile)
            if browse and not update:
                browse_file(mep)
    else:
        # do them all
        meptxt = []
        html = []
        files = glob.glob("mep-*.txt")
        files.sort()
        for file in files:
            meptxt.append(file)
            newfile = make_html(file, verbose=verbose)
            if newfile:
                html.append(newfile)
        if browse and not update:
            browse_file("0")

    if update:
        push_mep(html, meptxt, username, verbose, local=local)
        if browse:
            if args:
                for mep in args:
                    browse_remote(mep)
            else:
                browse_remote("0")



if __name__ == "__main__":
    main()
