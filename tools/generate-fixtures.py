#!/usr/bin/env python3
"""Regenerate native parity fixtures using Go; never needed for a native build.

Usage: python3 tools/generate-fixtures.py [path/to/bamboo-core]
The original Go unit tests must pass before fixtures are accepted.
"""
import pathlib
import shutil
import subprocess
import sys
import tempfile

repo = pathlib.Path(__file__).resolve().parents[1]
core = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else repo.parent / 'bamboo-core'
fixtures = repo / 'bamboo/native/tests'
recorder = r'''
package bamboo
import ("fmt"; "os"; "testing"; "math/rand"; "sort")
var trace, _ = os.Create(os.Getenv("BAMBOO_TRACE"))
type recordingEngine struct { engine IEngine }
func NewEngine(im InputMethod, flags uint) IEngine {
 fmt.Fprintf(trace, "I\t%x\t%d\n", im.Name, flags)
 return &recordingEngine{newOriginalEngine(im, flags)}
}
func (r *recordingEngine) snapshot() {
 for _, mode := range []Mode{VietnameseMode, EnglishMode, ToneLess|LowerCase, MarkLess, FullText|VietnameseMode, FullText|EnglishMode, PunctuationMode, PunctuationMode|EnglishMode} {
  fmt.Fprintf(trace,"G\t%d\t%x\n",mode,r.engine.GetProcessedString(mode))
 }
 for _, complete := range []bool{false,true} {fmt.Fprintf(trace,"V\t%t\t%t\n",complete,r.engine.IsValid(complete))}
}
func (r *recordingEngine) SetFlag(f uint) {fmt.Fprintf(trace,"F\t%d\n",f);r.engine.SetFlag(f);r.snapshot()}
func (r *recordingEngine) GetInputMethod() InputMethod {return r.engine.GetInputMethod()}
func (r *recordingEngine) ProcessKey(k rune,m Mode) {fmt.Fprintf(trace,"K\t%d\t%d\n",k,m);r.engine.ProcessKey(k,m);r.snapshot()}
func (r *recordingEngine) ProcessString(s string,m Mode) { for _,k := range s {r.ProcessKey(k,m)} }
func (r *recordingEngine) GetProcessedString(m Mode) string {s:=r.engine.GetProcessedString(m);fmt.Fprintf(trace,"G\t%d\t%x\n",m,s);return s}
func (r *recordingEngine) IsValid(c bool) bool {v:=r.engine.IsValid(c);fmt.Fprintf(trace,"V\t%t\t%t\n",c,v);return v}
func (r *recordingEngine) CanProcessKey(k rune) bool {v:=r.engine.CanProcessKey(k);fmt.Fprintf(trace,"C\t%d\t%t\n",k,v);return v}
func (r *recordingEngine) RemoveLastChar(b bool) {fmt.Fprintf(trace,"D\t%t\n",b);r.engine.RemoveLastChar(b);r.snapshot()}
func (r *recordingEngine) RestoreLastWord(b bool) {fmt.Fprintf(trace,"R\t%t\n",b);r.engine.RestoreLastWord(b);r.snapshot()}
func (r *recordingEngine) Reset() {fmt.Fprintln(trace,"Z");r.engine.Reset();r.snapshot()}
func TestNativeCorpus(t *testing.T) {
 rng:=rand.New(rand.NewSource(73))
 names:=[]string{};for name:=range InputMethodDefinitions {names=append(names,name)};sort.Strings(names)
 corpus:=[]string{"chuaarn","uwow","Thuow","giamaf","chuyeern","ddafi","oorr","nguowfi","hoaf","HIEEUR","giw","[]","ww","d[]ng9 t4i","tieengs Vieetj","Đặng Việt 🙂","uowng","quyeesn","aazz"}
 for _, name := range names {
  for _, flags:=range []uint{EstdFlags,EstdFlags&^EstdToneStyle,EstdFlags&^EfreeToneMarking} {
   r:=NewEngine(ParseInputMethod(InputMethodDefinitions,name),flags)
   r.SetFlag(flags)
   for _,s:=range corpus {r.Reset();r.ProcessString(s,VietnameseMode);r.RestoreLastWord(false);r.RestoreLastWord(true);r.RemoveLastChar(true);r.RemoveLastChar(false)}
   alphabet:=[]rune("abcdeghiklmnopqrstuvxyzw0123456789[]{}'`?~.^+*(éèçàƯĐ")
   for i:=0;i<40;i++ {r.Reset();for j:=0;j<12;j++ {r.ProcessKey(alphabet[rng.Intn(len(alphabet))],VietnameseMode)}; r.RemoveLastChar(true)}
   r.Reset();r.ProcessString("aw",EnglishMode|InReverseOrder)
   for _,c:=range alphabet {r.CanProcessKey(c)}
  }
 }
 utility,_:=os.Create(os.Getenv("BAMBOO_UTILITY"));defer utility.Close()
 for c:=rune(0);c<=0x1eff;c++ {
  if c>127 && !(c>=0xc0&&c<=0x1ff) && c<0x1e00 {continue}
  fmt.Fprintf(utility,"%d\t%d\t%d\t%t\t%t\t%t\n",c,FindVowelPosition(c),FindMarkPosition(c),IsVowel(c),IsVietnameseRune(c),IsWordBreakSymbol(c))
  for i:=0;i<6;i++ {fmt.Fprintf(utility,"T\t%d\t%d\t%d\n",c,i,AddToneToChar(c,uint8(i)))}
  for i:=0;i<5;i++ {fmt.Fprintf(utility,"M\t%d\t%d\t%d\n",c,i,AddMarkToChar(c,uint8(i)))}
 }
 charset,_:=os.Create(os.Getenv("BAMBOO_CHARSETS"));defer charset.Close()
 for _,name:=range GetCharsetNames() {
  text:="ASCII ĐƯƠ 012🙂"+string(Vowels);keys:=[]int{};for c:=range charsetDefinitions[name] {keys=append(keys,int(c))};sort.Ints(keys);for _,c:=range keys {text+=string(rune(c))}
  fmt.Fprintf(charset,"%x\t%x\t%x\n",name,text,Encode(name,text))
 }
}
'''
with tempfile.TemporaryDirectory(prefix='bamboo-reference-') as tmp:
    tmp = pathlib.Path(tmp)
    for source in core.glob('*.go'):
        text = source.read_text()
        if source.name == 'bamboo.go':
            text = text.replace('func NewEngine(', 'func newOriginalEngine(')
        (tmp / source.name).write_text(text)
    (tmp / 'go.mod').write_text('module bamboo-reference\n\ngo 1.18\n')
    (tmp / 'recording_test.go').write_text(recorder)
    import os
    env = dict(os.environ, BAMBOO_TRACE=str(tmp/'core.tsv'), BAMBOO_UTILITY=str(tmp/'utility.tsv'), BAMBOO_CHARSETS=str(tmp/'charsets.tsv'))
    subprocess.run(['go','test','-count=1','./...'],cwd=tmp,env=env,check=True)
    for name in ['core','utility','charsets']:
        shutil.copyfile(tmp/(name+'.tsv'),fixtures/(name+'.tsv'))
print('Updated core, utility, and charset fixtures from original Go tests and deterministic corpus.')

adapter_recorder = r'''
package main
import ("bamboo-core"; "fmt"; "os"; "testing"; "math/rand"; "sort")
func TestAdapterFixtures(t *testing.T) {
 out,_:=os.Create(os.Getenv("BAMBOO_ADAPTER"));defer out.Close()
 dictionary:=map[string]bool{"chuẩn":true,"tiếng":true,"việt":true,"đài":true}
 macros:=&MacroTable{mTable:map[string]string{"btw":"By The Way","vn":"Việt Nam"}}
 names:=[]string{};for name:=range bamboo.InputMethodDefinitions {names=append(names,name)};sort.Strings(names);names=append(names,"Custom")
 rng:=rand.New(rand.NewSource(91))
 for _,name:=range names {
  definitions:=bamboo.InputMethodDefinitions
  if name=="Custom" {definitions=map[string]bamboo.InputMethodDefinition{"Custom":{"q":"__ươ","w":"UOA_ƯƠĂ__Ư","s":"DauSac"}}}
  e:=&FcitxBambooEngine{preeditor:bamboo.NewEngine(bamboo.ParseInputMethod(definitions,name),bamboo.EstdFlags),dictionary:dictionary,macroTable:macros}
  fmt.Fprintf(out,"I\t%x\n",name)
  for _,mask:=range []uint{83,81,67,95,115,19} {
   for _,charset:=range []string{"Unicode","TCVN3 (ABC)"} {
    e.autoNonVnRestore=mask&1!=0;e.ddFreeStyle=mask&2!=0;e.macroEnabled=mask&4!=0;e.autoCapitalizeMacro=mask&8!=0;e.spellCheckWithDicts=mask&16!=0;e.outputCharset=charset
    flags:=uint(bamboo.EstdFlags);if mask&32!=0 {flags &^= bamboo.EstdToneStyle};if mask&64==0 {flags &^= bamboo.EfreeToneMarking};e.preeditor.SetFlag(flags)
    fmt.Fprintf(out,"O\t%d\t%x\n",mask,charset)
    key:=func(k rune,state uint32) {accepted:=e.preeditProcessKeyEvent(uint32(k),state);fmt.Fprintf(out,"K\t%d\t%d\t%t\t%x\t%x\n",k,state,accepted,e.preeditText,e.commitText);e.commitText=""}
    reset:=func(){e.commitPreeditAndReset("");fmt.Fprintln(out,"Z")}
    for _,s:=range []string{"chuaarn ","ddafi ","tieengs Vieetj ","btw ","BTW\t","vn\t","aw","uwow","ww","[]","{]","Đặng Việt","q","abc123.","chuyeern"} {
     reset();for _,k:=range s {if k=='\t' {key(0xff09,0)} else {key(k,0)}}
     key(0xff08,0);key(0xff08,0);e.shouldRestoreKeyStrokes=true;fmt.Fprintln(out,"R");key(' ',1);key('a',4)
    }
    alphabet:=[]rune("abcdeghiklmnopqrstuvxyzw0123456789[]{}'`?~.^+*(éèçàƯĐ")
    for i:=0;i<8;i++ {reset();for j:=0;j<12;j++ {key(alphabet[rng.Intn(len(alphabet))],0)};key(0xff08,0);key(' ',0)}
    reset();key('[',2);key(']',2);key(' ',0)
    reset();for _,k:=range "chuaarn" {key(k,0)};e.commitPreeditAndReset(e.getPreeditString());fmt.Fprintf(out,"B\t%x\n",e.commitText);e.commitText=""
   }
  }
 }
}
'''
with tempfile.TemporaryDirectory(prefix='bamboo-adapter-reference-') as tmp:
    tmp = pathlib.Path(tmp)
    for name in ['fcitxbambooengine.go','engine_utils.go','macrotable.go','utils.go']:
        shutil.copyfile(repo/'bamboo'/name,tmp/name)
    (tmp/'go.mod').write_text(f'module adapter-reference\n\ngo 1.18\nrequire bamboo-core v0.0.0\nreplace bamboo-core => {core}\n')
    (tmp/'recording_test.go').write_text(adapter_recorder)
    env = dict(os.environ, BAMBOO_ADAPTER=str(tmp/'adapter.tsv'))
    subprocess.run(['go','test','-count=1','./...'],cwd=tmp,env=env,check=True)
    shutil.copyfile(tmp/'adapter.tsv',fixtures/'adapter.tsv')
print('Updated adapter event fixtures from the original Go adapter.')
