makefile(
   uri     : 'x-oz://system/wp'
   depends : o(
		'QTk.ozf'
		: ['QTkFrame.ozf' 'QTkBare.ozf' 'QTkDevel.ozf' 'QTkImage.ozf' 'QTkMenu.ozf'
		   'QTkSpace.ozf' 'QTkLabel.ozf' 'QTkButton.ozf' 'QTkCheckbutton.ozf'
		   'QTkRadiobutton.ozf' 'QTkScale.ozf' 'QTkScrollbar.ozf' 'QTkEntry.ozf'
		   'QTkCanvas.ozf' 'QTkListbox.ozf' 'QTkText.ozf' 'QTkPlaceholder.ozf' 'QTkGrid.ozf'
		   'QTkPanel.ozf' 'QTkRubberframe.ozf' 'QTkScrollframe.ozf' 'QTkToolbar.ozf'
		   'QTkDropdownlistbox.ozf' 'QTkNumberentry.ozf' 'QTkMigratable.ozf'
		   'PrintCanvas.ozf' 'GetSignature.ozf' 'QTkFont.ozf' 'Session.ozf']
		'QTkBare.ozf'
		: ['QTk.oz' 'QTkClipboard.oz' 'QTkFont.oz' 'QTkDialogbox.oz'
		   'QTkFrame.oz']
		'QTkDropdownbutton_bitmap.ozf'
		: ['QTkImageLibBoot.ozf' 'mini-down.xbm']
		'QTkDropdownlistbox.ozf'
		: ['QTkBare.ozf' 'QTkImage.ozf' 'QTkDevel.ozf' 'QTkDropdownbutton_bitmap.ozf']
		'QTkNumberentry.ozf'
		: ['QTkNumberentry_bitmap.ozf']
		'QTkNumberentry_bitmap.ozf'
		: ['QTkImageLibBoot.ozf' 'mini-inc.xbm' 'mini-dec.xbm']
                'Prototyper.ozf'
                : ['Prototyper.oz'])
   lib     : ['QTk.ozf' 'QTkDevel.ozf' 'QTkBare.ozf' 'Prototyper.ozf']
   rules   : o(
		'QTk.ozf' : ozl('QTkBare.ozf'))
   doc     : [
	      'html/index.html'
	      'html/node10.html'
	      'html/node11.html'
	      'html/node12.html'
	      'html/node13.html'
	      'html/node14.html'
	      'html/node15.html'
	      'html/node16.html'
	      'html/node17.html'
	      'html/node18.html'
	      'html/node19.html'
	      'html/node1.html'
	      'html/node20.html'
	      'html/node21.html'
	      'html/node22.html'
	      'html/node23.html'
	      'html/node24.html'
	      'html/node25.html'
	      'html/node26.html'
	      'html/node27.html'
	      'html/node28.html'
	      'html/node29.html'
	      'html/node2.html'
	      'html/node30.html'
	      'html/node31.html'
	      'html/node32.html'
	      'html/node33.html'
	      'html/node34.html'
	      'html/node35.html'
	      'html/node36.html'
	      'html/node37.html'
	      'html/node38.html'
	      'html/node39.html'
	      'html/node3.html'
	      'html/node40.html'
	      'html/node41.html'
	      'html/node4.html'
	      'html/node5.html'
	      'html/node6.html'
	      'html/node7.html'
	      'html/node8.html'
	      'html/node9.html'
	      'html/ozdoc.css'
	      'html/page.gif'
	      'html/picture100.gif'
	      'html/picture10.gif'
	      'html/picture11.gif'
	      'html/picture1.gif'
	      'html/picture2.gif'
	      'html/picture3.gif'
	      'html/picture4.gif'
	      'html/picture5.gif'
	      'html/picture6.gif'
	      'html/picture8.gif'
	      'html/picture9.gif'
	     ]
   )
