; z80dasm 1.2.0
; command line: z80dasm --origin=0x6800 --address --labels --source -o original/blocks/03_main.asm original/blocks/03_DATA_headless.dat.bin

	org 06800h

	di			;6800	f3		.
	ld sp,06000h		;6801	31 00 60	1 . `
	xor a			;6804	af		.
	out (0feh),a		;6805	d3 fe		. .
	ld hl,0f200h		;6807	21 00 f2	! . .
	ld b,001h		;680a	06 01		. .
l680ch:
	ld c,000h		;680c	0e 00		. .
l680eh:
	ld d,c			;680e	51		Q
	ld e,b			;680f	58		X
	xor a			;6810	af		.
l6811h:
	srl d			;6811	cb 3a		. :
	rra			;6813	1f		.
	dec e			;6814	1d		.
	jr nz,l6811h		;6815	20 fa		  .
	inc h			;6817	24		$
	ld (hl),a		;6818	77		w
	dec h			;6819	25		%
	ld (hl),d		;681a	72		r
	inc hl			;681b	23		#
	inc c			;681c	0c		.
	jr nz,l680eh		;681d	20 ef		  .
	inc h			;681f	24		$
	inc b			;6820	04		.
	bit 3,b			;6821	cb 58		. X
	jr z,l680ch		;6823	28 e7		( .
	call sub_6853h		;6825	cd 53 68	. S h
	ld ix,l7796h		;6828	dd 21 96 77	. ! . w
l682ch:
	ld l,(ix+000h)		;682c	dd 6e 00	. n .
	ld h,(ix+001h)		;682f	dd 66 01	. f .
	ld a,l			;6832	7d		}
	or h			;6833	b4		.
	jr z,l684bh		;6834	28 15		( .
	inc ix			;6836	dd 23		. #
	inc ix			;6838	dd 23		. #
	ld c,(hl)		;683a	4e		N
	inc hl			;683b	23		#
	ld e,(hl)		;683c	5e		^
	inc hl			;683d	23		#
l683eh:
	ld b,c			;683e	41		A
l683fh:
	ld a,(hl)		;683f	7e		~
	inc hl			;6840	23		#
	xor (hl)		;6841	ae		.
	ld (hl),a		;6842	77		w
	inc hl			;6843	23		#
	djnz l683fh		;6844	10 f9		. .
	dec e			;6846	1d		.
	jr nz,l683eh		;6847	20 f5		  .
	jr l682ch		;6849	18 e1		. .
l684bh:
	ld a,00ch		;684b	3e 0c		> .
	ld (l891dh),a		;684d	32 1d 89	2 . .
	jp lb9b1h		;6850	c3 b1 b9	. . .
sub_6853h:
	ld ix,l68d7h		;6853	dd 21 d7 68	. ! . h
l6857h:
	ld l,(ix+000h)		;6857	dd 6e 00	. n .
	ld h,(ix+001h)		;685a	dd 66 01	. f .
	ld a,h			;685d	7c		|
	or l			;685e	b5		.
	ret z			;685f	c8		.
	ld c,(ix+002h)		;6860	dd 4e 02	. N .
	srl c			;6863	cb 39		. 9
	ld e,(ix+003h)		;6865	dd 5e 03	. ^ .
	ld d,(ix+004h)		;6868	dd 56 04	. V .
	ld a,001h		;686b	3e 01		> .
	ld (068a5h),a		;686d	32 a5 68	2 . h
l6870h:
	srl c			;6870	cb 39		. 9
	push bc			;6872	c5		.
	call c,sub_688bh	;6873	dc 8b 68	. . h
	pop bc			;6876	c1		.
	ld a,c			;6877	79		y
	and a			;6878	a7		.
	jr z,l6884h		;6879	28 09		( .
	ld a,(068a5h)		;687b	3a a5 68	: . h
	inc a			;687e	3c		<
	ld (068a5h),a		;687f	32 a5 68	2 . h
	jr l6870h		;6882	18 ec		. .
l6884h:
	ld de,00005h		;6884	11 05 00	. . .
	add ix,de		;6887	dd 19		. .
	jr l6857h		;6889	18 cc		. .
sub_688bh:
	push hl			;688b	e5		.
	ld a,(hl)		;688c	7e		~
	ld (0689ah),a		;688d	32 9a 68	2 . h
	inc a			;6890	3c		<
	ld (de),a		;6891	12		.
	inc hl			;6892	23		#
	inc de			;6893	13		.
	ld a,(hl)		;6894	7e		~
	ld (de),a		;6895	12		.
	inc hl			;6896	23		#
	inc de			;6897	13		.
l6898h:
	ex af,af'		;6898	08		.
	ld b,000h		;6899	06 00		. .
l689bh:
	push bc			;689b	c5		.
	push de			;689c	d5		.
	ld a,(hl)		;689d	7e		~
	inc hl			;689e	23		#
	ld e,(hl)		;689f	5e		^
	inc hl			;68a0	23		#
	ld d,000h		;68a1	16 00		. .
	ld c,d			;68a3	4a		J
	ld b,000h		;68a4	06 00		. .
l68a6h:
	srl e			;68a6	cb 3b		. ;
	rr d			;68a8	cb 1a		. .
	srl a			;68aa	cb 3f		. ?
	rr c			;68ac	cb 19		. .
	djnz l68a6h		;68ae	10 f6		. .
	ld b,a			;68b0	47		G
	ld a,d			;68b1	7a		z
	ld (068c9h),a		;68b2	32 c9 68	2 . h
	ld a,e			;68b5	7b		{
	ld (068c0h),a		;68b6	32 c0 68	2 . h
	pop de			;68b9	d1		.
	ld a,(de)		;68ba	1a		.
	or b			;68bb	b0		.
	ld (de),a		;68bc	12		.
	inc de			;68bd	13		.
	ld a,(de)		;68be	1a		.
	or 000h			;68bf	f6 00		. .
	ld (de),a		;68c1	12		.
	inc de			;68c2	13		.
	ld a,(de)		;68c3	1a		.
	or c			;68c4	b1		.
	ld (de),a		;68c5	12		.
	inc de			;68c6	13		.
	ld a,(de)		;68c7	1a		.
	or 000h			;68c8	f6 00		. .
	ld (de),a		;68ca	12		.
	dec de			;68cb	1b		.
	pop bc			;68cc	c1		.
	djnz l689bh		;68cd	10 cc		. .
	inc de			;68cf	13		.
	inc de			;68d0	13		.
	ex af,af'		;68d1	08		.
	dec a			;68d2	3d		=
	jr nz,l6898h		;68d3	20 c3		  .
	pop hl			;68d5	e1		.
	ret			;68d6	c9		.
l68d7h:
	ld d,07bh		;68d7	16 7b		. {
	rst 38h			;68d9	ff		.
	ld c,b			;68da	48		H
	ld a,e			;68db	7b		{
l68dch:
	jr c,l695ch		;68dc	38 7e		8 ~
	djnz $+80		;68de	10 4e		. N
	ld a,l			;68e0	7d		}
	ld b,d			;68e1	42		B
	ld a,a			;68e2	7f		.
	djnz $-116		;68e3	10 8a		. .
	add a,d			;68e5	82		.
	adc a,b			;68e6	88		.
	add a,c			;68e7	81		.
	djnz l68dch		;68e8	10 f2		. .
	add a,c			;68ea	81		.
	nop			;68eb	00		.
	nop			;68ec	00		.
	inc bc			;68ed	03		.
	ex af,af'		;68ee	08		.
	inc a			;68ef	3c		<
	nop			;68f0	00		.
	cp 000h			;68f1	fe 00		. .
	call m,sub_7c00h	;68f3	fc 00 7c	. . |
l68f6h:
	jr l68f6h		;68f6	18 fe		. .
	ld l,h			;68f8	6c		l
	rst 38h			;68f9	ff		.
	ld a,h			;68fa	7c		|
	ld a,h			;68fb	7c		|
l68fch:
	jr c,l68fch		;68fc	38 fe		8 .
	ld l,h			;68fe	6c		l
	rst 38h			;68ff	ff		.
	ld h,(hl)		;6900	66		f
	ld a,h			;6901	7c		|
l6902h:
	jr l6902h		;6902	18 fe		. .
	ld l,h			;6904	6c		l
	rst 38h			;6905	ff		.
	ld h,(hl)		;6906	66		f
	inc a			;6907	3c		<
l6908h:
	jr l6908h		;6908	18 fe		. .
	ld l,h			;690a	6c		l
	rst 38h			;690b	ff		.
	ld a,h			;690c	7c		|
	ld a,(hl)		;690d	7e		~
l690eh:
	jr l690eh		;690e	18 fe		. .
	ld a,h			;6910	7c		|
	cp 060h			;6911	fe 60		. `
	ld a,(hl)		;6913	7e		~
	inc a			;6914	3c		<
	cp 038h			;6915	fe 38		. 8
	ret p			;6917	f0		.
	ld h,b			;6918	60		`
	ld a,(hl)		;6919	7e		~
	nop			;691a	00		.
	ld a,h			;691b	7c		|
	nop			;691c	00		.
	ret p			;691d	f0		.
	nop			;691e	00		.
	inc bc			;691f	03		.
	ex af,af'		;6920	08		.
	ld a,h			;6921	7c		|
	nop			;6922	00		.
	cp 000h			;6923	fe 00		. .
	cp 000h			;6925	fe 00		. .
	cp 038h			;6927	fe 38		. 8
	cp 06ch			;6929	fe 6c		. l
	rst 38h			;692b	ff		.
	ld a,h			;692c	7c		|
	cp 07ch			;692d	fe 7c		. |
	cp 06ch			;692f	fe 6c		. l
	rst 38h			;6931	ff		.
	ld h,(hl)		;6932	66		f
	cp 06ch			;6933	fe 6c		. l
	cp 06ch			;6935	fe 6c		. l
	rst 38h			;6937	ff		.
	ld h,(hl)		;6938	66		f
	cp 018h			;6939	fe 18		. .
	cp 06ch			;693b	fe 6c		. l
	rst 38h			;693d	ff		.
	ld a,h			;693e	7c		|
	cp 030h			;693f	fe 30		. 0
	cp 07ch			;6941	fe 7c		. |
	cp 060h			;6943	fe 60		. `
	cp 07ch			;6945	fe 7c		. |
	cp 038h			;6947	fe 38		. 8
	ret p			;6949	f0		.
	ld h,b			;694a	60		`
	cp 000h			;694b	fe 00		. .
	ld a,h			;694d	7c		|
	nop			;694e	00		.
	ret p			;694f	f0		.
	nop			;6950	00		.
	ld (bc),a		;6951	02		.
	ex af,af'		;6952	08		.
	ld a,a			;6953	7f		.
	nop			;6954	00		.
	cp h			;6955	bc		.
	nop			;6956	00		.
	ld a,a			;6957	7f		.
	inc sp			;6958	33		3
	cp h			;6959	bc		.
	jr $+129		;695a	18 7f		. .
l695ch:
	inc sp			;695c	33		3
	cp h			;695d	bc		.
	jr l69dfh		;695e	18 7f		. .
	ccf			;6960	3f		?
	cp h			;6961	bc		.
	jr l69e3h		;6962	18 7f		. .
	ccf			;6964	3f		?
	cp h			;6965	bc		.
	jr l69e7h		;6966	18 7f		. .
	inc sp			;6968	33		3
	cp h			;6969	bc		.
	jr l69ebh		;696a	18 7f		. .
	inc sp			;696c	33		3
	cp h			;696d	bc		.
	jr $+129		;696e	18 7f		. .
	nop			;6970	00		.
	cp h			;6971	bc		.
	nop			;6972	00		.
	ld a,(bc)		;6973	0a		.
	ex af,af'		;6974	08		.
l6975h:
	ld a,h			;6975	7c		|
	nop			;6976	00		.
	cp 038h			;6977	fe 38		. 8
	cp 06ch			;6979	fe 6c		. l
	cp 06ch			;697b	fe 6c		. l
	cp 06ch			;697d	fe 6c		. l
	cp 06ch			;697f	fe 6c		. l
	cp 038h			;6981	fe 38		. 8
	ld a,h			;6983	7c		|
	nop			;6984	00		.
	inc a			;6985	3c		<
	nop			;6986	00		.
	ld a,h			;6987	7c		|
	jr l6a06h		;6988	18 7c		. |
	jr c,$+126		;698a	38 7c		8 |
	jr $+62			;698c	18 3c		. <
	jr $+128		;698e	18 7e		. ~
	jr l6a10h		;6990	18 7e		. ~
	inc a			;6992	3c		<
	ld a,(hl)		;6993	7e		~
	nop			;6994	00		.
	ld a,h			;6995	7c		|
	nop			;6996	00		.
	cp 038h			;6997	fe 38		. 8
	cp 07ch			;6999	fe 7c		. |
	cp 06ch			;699b	fe 6c		. l
	cp 018h			;699d	fe 18		. .
	cp 030h			;699f	fe 30		. 0
	cp 07ch			;69a1	fe 7c		. |
	cp 000h			;69a3	fe 00		. .
	ld a,h			;69a5	7c		|
	nop			;69a6	00		.
	cp 038h			;69a7	fe 38		. 8
	cp 06ch			;69a9	fe 6c		. l
	cp 018h			;69ab	fe 18		. .
	cp 018h			;69ad	fe 18		. .
	cp 06ch			;69af	fe 6c		. l
	cp 038h			;69b1	fe 38		. 8
	ld a,h			;69b3	7c		|
	nop			;69b4	00		.
	inc a			;69b5	3c		<
	nop			;69b6	00		.
	ld a,h			;69b7	7c		|
l69b8h:
	jr $+126		;69b8	18 7c		. |
	jr c,l69b8h		;69ba	38 fc		8 .
l69bch:
	jr z,l69bch		;69bc	28 fe		( .
	ld l,b			;69be	68		h
	cp 07ch			;69bf	fe 7c		. |
	cp 018h			;69c1	fe 18		. .
	inc a			;69c3	3c		<
	nop			;69c4	00		.
	cp 000h			;69c5	fe 00		. .
	cp 07ch			;69c7	fe 7c		. |
	cp 060h			;69c9	fe 60		. `
	cp 078h			;69cb	fe 78		. x
	cp 00ch			;69cd	fe 0c		. .
	cp 06ch			;69cf	fe 6c		. l
	cp 038h			;69d1	fe 38		. 8
	ld a,h			;69d3	7c		|
	nop			;69d4	00		.
	inc a			;69d5	3c		<
l69d6h:
	nop			;69d6	00		.
	ld a,h			;69d7	7c		|
	jr l69d6h		;69d8	18 fc		. .
l69dah:
	jr nc,l69dah		;69da	30 fe		0 .
	ld a,b			;69dc	78		x
	cp 06ch			;69dd	fe 6c		. l
l69dfh:
	cp 06ch			;69df	fe 6c		. l
	cp 038h			;69e1	fe 38		. 8
l69e3h:
	ld a,h			;69e3	7c		|
	nop			;69e4	00		.
	cp 000h			;69e5	fe 00		. .
l69e7h:
	cp 07ch			;69e7	fe 7c		. |
	cp 07ch			;69e9	fe 7c		. |
l69ebh:
	cp 018h			;69eb	fe 18		. .
	ld a,h			;69ed	7c		|
	jr c,l6a6ch		;69ee	38 7c		8 |
	jr nc,$+122		;69f0	30 78		0 x
	jr nc,l6a6ch		;69f2	30 78		0 x
	nop			;69f4	00		.
	ld a,h			;69f5	7c		|
	nop			;69f6	00		.
	cp 038h			;69f7	fe 38		. 8
	cp 06ch			;69f9	fe 6c		. l
	cp 038h			;69fb	fe 38		. 8
	cp 06ch			;69fd	fe 6c		. l
	cp 06ch			;69ff	fe 6c		. l
	cp 038h			;6a01	fe 38		. 8
	ld a,h			;6a03	7c		|
	nop			;6a04	00		.
	ld a,h			;6a05	7c		|
l6a06h:
	nop			;6a06	00		.
	cp 038h			;6a07	fe 38		. 8
	cp 06ch			;6a09	fe 6c		. l
	cp 06ch			;6a0b	fe 6c		. l
	cp 03ch			;6a0d	fe 3c		. <
	ld a,(hl)		;6a0f	7e		~
l6a10h:
	jr $+126		;6a10	18 7c		. |
	jr nc,l6a8ch		;6a12	30 78		0 x
	nop			;6a14	00		.
	ld a,h			;6a15	7c		|
	and 0d2h		;6a16	e6 d2		. .
	jp z,l7ce6h		;6a18	ca e6 7c	. . |
	jr $+58			;6a1b	18 38		. 8
	jr $+26			;6a1d	18 18		. .
	jr $+62			;6a1f	18 3c		. <
	ld a,h			;6a21	7c		|
	cp 0c6h			;6a22	fe c6		. .
	inc e			;6a24	1c		.
	ld (hl),b		;6a25	70		p
	cp 07ch			;6a26	fe 7c		. |
	add a,01eh		;6a28	c6 1e		. .
	ld e,0c6h		;6a2a	1e c6		. .
	ld a,h			;6a2c	7c		|
	inc c			;6a2d	0c		.
	inc e			;6a2e	1c		.
	inc (hl)		;6a2f	34		4
	ld h,h			;6a30	64		d
	cp 004h			;6a31	fe 04		. .
	cp 0c0h			;6a33	fe c0		. .
	call m,0c606h		;6a35	fc 06 c6	. . .
	ld a,h			;6a38	7c		|
	ld c,038h		;6a39	0e 38		. 8
	ld l,h			;6a3b	6c		l
	add a,0c6h		;6a3c	c6 c6		. .
	ld a,h			;6a3e	7c		|
	cp 0feh			;6a3f	fe fe		. .
	inc c			;6a41	0c		.
	inc c			;6a42	0c		.
	jr $+26			;6a43	18 18		. .
	ld a,h			;6a45	7c		|
	add a,07ch		;6a46	c6 7c		. |
	add a,0c6h		;6a48	c6 c6		. .
	ld a,h			;6a4a	7c		|
	ld a,h			;6a4b	7c		|
	add a,0c6h		;6a4c	c6 c6		. .
	ld a,(hl)		;6a4e	7e		~
	ld b,006h		;6a4f	06 06		. .
	ld a,h			;6a51	7c		|
	cp 0c6h			;6a52	fe c6		. .
	cp 0c6h			;6a54	fe c6		. .
	add a,0f8h		;6a56	c6 f8		. .
	call z,0ccf8h		;6a58	cc f8 cc	. . .
	call z,sub_7cf8h	;6a5b	cc f8 7c	. . |
	and 0c0h		;6a5e	e6 c0		. .
	ret nz			;6a60	c0		.
	and 07ch		;6a61	e6 7c		. |
	call m,0c6c6h		;6a63	fc c6 c6	. . .
	add a,0c6h		;6a66	c6 c6		. .
	call m,0c0feh		;6a68	fc fe c0	. . .
	ret m			;6a6b	f8		.
l6a6ch:
	ret m			;6a6c	f8		.
	ret nz			;6a6d	c0		.
	cp 0feh			;6a6e	fe fe		. .
	ret nz			;6a70	c0		.
	ret m			;6a71	f8		.
	ret m			;6a72	f8		.
	ret nz			;6a73	c0		.
	ret nz			;6a74	c0		.
	ld a,h			;6a75	7c		|
	and 0c0h		;6a76	e6 c0		. .
	adc a,0e6h		;6a78	ce e6		. .
	ld a,h			;6a7a	7c		|
	add a,0c6h		;6a7b	c6 c6		. .
	cp 0feh			;6a7d	fe fe		. .
	add a,0c6h		;6a7f	c6 c6		. .
	jr nc,$+50		;6a81	30 30		0 0
	jr nc,$+50		;6a83	30 30		0 0
	jr nc,l6ab7h		;6a85	30 30		0 0
	ld a,h			;6a87	7c		|
	ld a,h			;6a88	7c		|
	jr $+26			;6a89	18 18		. .
	ret c			;6a8b	d8		.
l6a8ch:
	ld (hl),b		;6a8c	70		p
	add a,0dch		;6a8d	c6 dc		. .
	ret m			;6a8f	f8		.
	ret m			;6a90	f8		.
	call z,sub_c0c6h	;6a91	cc c6 c0	. . .
	ret nz			;6a94	c0		.
	ret nz			;6a95	c0		.
	ret nz			;6a96	c0		.
	cp 0feh			;6a97	fe fe		. .
	ld a,h			;6a99	7c		|
	cp 0d6h			;6a9a	fe d6		. .
	sub 0d6h		;6a9c	d6 d6		. .
	sub 0fch		;6a9e	d6 fc		. .
	cp 0c6h			;6aa0	fe c6		. .
	add a,0c6h		;6aa2	c6 c6		. .
	add a,07ch		;6aa4	c6 7c		. |
	cp 0c6h			;6aa6	fe c6		. .
	add a,0feh		;6aa8	c6 fe		. .
	ld a,h			;6aaa	7c		|
	call m,0c6c6h		;6aab	fc c6 c6	. . .
	call m,sub_c0c0h	;6aae	fc c0 c0	. . .
	ld a,h			;6ab1	7c		|
	cp 0c6h			;6ab2	fe c6		. .
	jp nz,l7af4h		;6ab4	c2 f4 7a	. . z
l6ab7h:
	call m,0c6c6h		;6ab7	fc c6 c6	. . .
	ret m			;6aba	f8		.
	call z,sub_7ec6h	;6abb	cc c6 7e	. . ~
	ret nz			;6abe	c0		.
	ld a,h			;6abf	7c		|
	ld b,0feh		;6ac0	06 fe		. .
	call m,0fcfch		;6ac2	fc fc fc	. . .
	jr nc,$+50		;6ac5	30 30		0 0
	jr nc,$+50		;6ac7	30 30		0 0
	add a,0c6h		;6ac9	c6 c6		. .
	add a,0c6h		;6acb	c6 c6		. .
	cp 07eh			;6acd	fe 7e		. ~
	add a,0c6h		;6acf	c6 c6		. .
	ld l,h			;6ad1	6c		l
	ld l,h			;6ad2	6c		l
	jr c,l6b0dh		;6ad3	38 38		8 8
	sub 0d6h		;6ad5	d6 d6		. .
	sub 0d6h		;6ad7	d6 d6		. .
	cp 07ch			;6ad9	fe 7c		. |
	add a,06ch		;6adb	c6 6c		. l
	jr c,l6b17h		;6add	38 38		8 8
	ld l,h			;6adf	6c		l
	add a,0c6h		;6ae0	c6 c6		. .
	add a,0feh		;6ae2	c6 fe		. .
	jr c,l6b1eh		;6ae4	38 38		8 8
l6ae6h:
	jr c,l6ae6h		;6ae6	38 fe		8 .
	call m,03018h		;6ae8	fc 18 30	. . 0
	ld a,(hl)		;6aeb	7e		~
	cp 000h			;6aec	fe 00		. .
	nop			;6aee	00		.
	nop			;6aef	00		.
	nop			;6af0	00		.
	jr l6b0bh		;6af1	18 18		. .
	nop			;6af3	00		.
	nop			;6af4	00		.
	nop			;6af5	00		.
	jr l6b10h		;6af6	18 18		. .
	jr nc,l6afah		;6af8	30 00		0 .
l6afah:
	nop			;6afa	00		.
	nop			;6afb	00		.
	nop			;6afc	00		.
	nop			;6afd	00		.
	nop			;6afe	00		.
	nop			;6aff	00		.
	nop			;6b00	00		.
	ld a,(hl)		;6b01	7e		~
	ld a,(hl)		;6b02	7e		~
	nop			;6b03	00		.
	nop			;6b04	00		.
	nop			;6b05	00		.
	nop			;6b06	00		.
	nop			;6b07	00		.
	nop			;6b08	00		.
	nop			;6b09	00		.
	rst 38h			;6b0a	ff		.
l6b0bh:
	cp 06ch			;6b0b	fe 6c		. l
l6b0dh:
	ld l,h			;6b0d	6c		l
	ld l,h			;6b0e	6c		l
	ld l,h			;6b0f	6c		l
l6b10h:
	cp 000h			;6b10	fe 00		. .
	nop			;6b12	00		.
	rst 38h			;6b13	ff		.
	nop			;6b14	00		.
	nop			;6b15	00		.
	rst 38h			;6b16	ff		.
l6b17h:
	ld bc,l9f20h		;6b17	01 20 9f	.   .
	nop			;6b1a	00		.
	nop			;6b1b	00		.
	nop			;6b1c	00		.
	nop			;6b1d	00		.
l6b1eh:
	rst 8			;6b1e	cf		.
	ld l,(hl)		;6b1f	6e		n
	inc a			;6b20	3c		<
	nop			;6b21	00		.
	nop			;6b22	00		.
	nop			;6b23	00		.
	nop			;6b24	00		.
	nop			;6b25	00		.
	nop			;6b26	00		.
	nop			;6b27	00		.
	nop			;6b28	00		.
	nop			;6b29	00		.
	nop			;6b2a	00		.
	nop			;6b2b	00		.
	nop			;6b2c	00		.
	nop			;6b2d	00		.
	nop			;6b2e	00		.
	nop			;6b2f	00		.
	nop			;6b30	00		.
	inc a			;6b31	3c		<
	ld l,(hl)		;6b32	6e		n
	rst 8			;6b33	cf		.
	nop			;6b34	00		.
	nop			;6b35	00		.
	nop			;6b36	00		.
	nop			;6b37	00		.
	sbc a,a			;6b38	9f		.
	ld bc,04504h		;6b39	01 04 45	. . E
	ld b,a			;6b3c	47		G
	ld b,a			;6b3d	47		G
	ld b,l			;6b3e	45		E
l6b3fh:
	ld bc,03c20h		;6b3f	01 20 3c	.   <
	ld l,(hl)		;6b42	6e		n
	rst 8			;6b43	cf		.
	sbc a,a			;6b44	9f		.
	sbc a,a			;6b45	9f		.
	sbc a,a			;6b46	9f		.
	sbc a,a			;6b47	9f		.
	ld h,b			;6b48	60		`
	sbc a,a			;6b49	9f		.
	sbc a,a			;6b4a	9f		.
	sbc a,a			;6b4b	9f		.
	sbc a,a			;6b4c	9f		.
	sbc a,a			;6b4d	9f		.
	sbc a,a			;6b4e	9f		.
	sbc a,a			;6b4f	9f		.
	sbc a,a			;6b50	9f		.
	sbc a,a			;6b51	9f		.
	sbc a,a			;6b52	9f		.
	sbc a,a			;6b53	9f		.
	sbc a,a			;6b54	9f		.
	sbc a,a			;6b55	9f		.
	sbc a,a			;6b56	9f		.
	sbc a,a			;6b57	9f		.
	sbc a,a			;6b58	9f		.
	ld h,b			;6b59	60		`
	sbc a,a			;6b5a	9f		.
	sbc a,a			;6b5b	9f		.
	sbc a,a			;6b5c	9f		.
	sbc a,a			;6b5d	9f		.
	rst 8			;6b5e	cf		.
	ld l,(hl)		;6b5f	6e		n
	inc a			;6b60	3c		<
	ld bc,04504h		;6b61	01 04 45	. . E
	dec b			;6b64	05		.
	dec b			;6b65	05		.
	ld b,l			;6b66	45		E
	ld bc,03c20h		;6b67	01 20 3c	.   <
	halt			;6b6a	76		v
	di			;6b6b	f3		.
	ld sp,hl		;6b6c	f9		.
	ld sp,hl		;6b6d	f9		.
	ld sp,hl		;6b6e	f9		.
	ld sp,hl		;6b6f	f9		.
	ld b,0f9h		;6b70	06 f9		. .
	ld sp,hl		;6b72	f9		.
	ld sp,hl		;6b73	f9		.
	ld sp,hl		;6b74	f9		.
	ld sp,hl		;6b75	f9		.
	ld sp,hl		;6b76	f9		.
	ld sp,hl		;6b77	f9		.
	ld sp,hl		;6b78	f9		.
	ld sp,hl		;6b79	f9		.
	ld sp,hl		;6b7a	f9		.
	ld sp,hl		;6b7b	f9		.
	ld sp,hl		;6b7c	f9		.
	ld sp,hl		;6b7d	f9		.
	ld sp,hl		;6b7e	f9		.
	ld sp,hl		;6b7f	f9		.
	ld sp,hl		;6b80	f9		.
	ld b,0f9h		;6b81	06 f9		. .
	ld sp,hl		;6b83	f9		.
	ld sp,hl		;6b84	f9		.
	ld sp,hl		;6b85	f9		.
	di			;6b86	f3		.
	halt			;6b87	76		v
	inc a			;6b88	3c		<
	ld bc,04504h		;6b89	01 04 45	. . E
	dec b			;6b8c	05		.
	dec b			;6b8d	05		.
	ld b,l			;6b8e	45		E
l6b8fh:
	ld bc,00018h		;6b8f	01 18 00	. . .
	inc a			;6b92	3c		<
	inc a			;6b93	3c		<
	inc a			;6b94	3c		<
	inc a			;6b95	3c		<
	inc a			;6b96	3c		<
	inc a			;6b97	3c		<
	inc a			;6b98	3c		<
	nop			;6b99	00		.
	ld e,(hl)		;6b9a	5e		^
	ld e,(hl)		;6b9b	5e		^
	ld e,(hl)		;6b9c	5e		^
	ld e,(hl)		;6b9d	5e		^
	ld e,(hl)		;6b9e	5e		^
	ld e,(hl)		;6b9f	5e		^
	nop			;6ba0	00		.
	inc a			;6ba1	3c		<
	inc a			;6ba2	3c		<
	inc a			;6ba3	3c		<
	inc a			;6ba4	3c		<
	inc a			;6ba5	3c		<
	inc a			;6ba6	3c		<
	inc a			;6ba7	3c		<
	nop			;6ba8	00		.
	ld bc,00703h		;6ba9	01 03 07	. . .
	ld b,a			;6bac	47		G
	rlca			;6bad	07		.
	ld bc,00018h		;6bae	01 18 00	. . .
	inc a			;6bb1	3c		<
	inc a			;6bb2	3c		<
	inc a			;6bb3	3c		<
	inc a			;6bb4	3c		<
	inc a			;6bb5	3c		<
	inc a			;6bb6	3c		<
	inc a			;6bb7	3c		<
	nop			;6bb8	00		.
	ld a,d			;6bb9	7a		z
	ld a,d			;6bba	7a		z
	ld a,d			;6bbb	7a		z
	ld a,d			;6bbc	7a		z
	ld a,d			;6bbd	7a		z
	ld a,d			;6bbe	7a		z
	nop			;6bbf	00		.
	inc a			;6bc0	3c		<
	inc a			;6bc1	3c		<
	inc a			;6bc2	3c		<
	inc a			;6bc3	3c		<
	inc a			;6bc4	3c		<
	inc a			;6bc5	3c		<
	inc a			;6bc6	3c		<
	nop			;6bc7	00		.
	ld bc,00703h		;6bc8	01 03 07	. . .
	ld b,a			;6bcb	47		G
	rlca			;6bcc	07		.
	inc b			;6bcd	04		.
	ex af,af'		;6bce	08		.
	sbc a,(hl)		;6bcf	9e		.
	rst 38h			;6bd0	ff		.
	rst 38h			;6bd1	ff		.
	ld l,h			;6bd2	6c		l
	sbc a,l			;6bd3	9d		.
	rst 38h			;6bd4	ff		.
	rst 38h			;6bd5	ff		.
	ld l,(hl)		;6bd6	6e		n
	sbc a,e			;6bd7	9b		.
	rst 38h			;6bd8	ff		.
	rst 38h			;6bd9	ff		.
	ld l,a			;6bda	6f		o
	sub a			;6bdb	97		.
	rst 38h			;6bdc	ff		.
	rst 38h			;6bdd	ff		.
	ld l,a			;6bde	6f		o
	xor a			;6bdf	af		.
	rst 38h			;6be0	ff		.
	rst 38h			;6be1	ff		.
	ld l,c			;6be2	69		i
	ret nc			;6be3	d0		.
	nop			;6be4	00		.
	nop			;6be5	00		.
	sub e			;6be6	93		.
	and b			;6be7	a0		.
	nop			;6be8	00		.
	nop			;6be9	00		.
	sub (hl)		;6bea	96		.
	ld a,a			;6beb	7f		.
	rst 38h			;6bec	ff		.
	rst 38h			;6bed	ff		.
	ld l,h			;6bee	6c		l
	inc b			;6bef	04		.
	ld bc,00505h		;6bf0	01 05 05	. . .
	dec b			;6bf3	05		.
	ld b,l			;6bf4	45		E
	inc b			;6bf5	04		.
	ex af,af'		;6bf6	08		.
	nop			;6bf7	00		.
	nop			;6bf8	00		.
	nop			;6bf9	00		.
	ld a,000h		;6bfa	3e 00		> .
	ld a,(hl)		;6bfc	7e		~
	nop			;6bfd	00		.
	ld a,(hl)		;6bfe	7e		~
	ld a,a			;6bff	7f		.
	ld a,(hl)		;6c00	7e		~
	cp 0feh			;6c01	fe fe		. .
	ld a,a			;6c03	7f		.
	ld a,(hl)		;6c04	7e		~
	cp 0feh			;6c05	fe fe		. .
	ld a,a			;6c07	7f		.
	ld a,(hl)		;6c08	7e		~
	cp 09eh			;6c09	fe 9e		. .
	ld a,a			;6c0b	7f		.
	nop			;6c0c	00		.
	cp 0c1h			;6c0d	fe c1		. .
	nop			;6c0f	00		.
	ld a,(hl)		;6c10	7e		~
	nop			;6c11	00		.
	ld h,c			;6c12	61		a
	nop			;6c13	00		.
	nop			;6c14	00		.
	nop			;6c15	00		.
	ld a,004h		;6c16	3e 04		> .
	ld bc,04707h		;6c18	01 07 47	. . G
	rlca			;6c1b	07		.
	ld b,l			;6c1c	45		E
	inc b			;6c1d	04		.
	ex af,af'		;6c1e	08		.
	rst 38h			;6c1f	ff		.
	rst 38h			;6c20	ff		.
	rst 38h			;6c21	ff		.
	ld a,h			;6c22	7c		|
	rst 38h			;6c23	ff		.
	rst 38h			;6c24	ff		.
	rst 38h			;6c25	ff		.
	ld a,(hl)		;6c26	7e		~
	rst 38h			;6c27	ff		.
	rst 38h			;6c28	ff		.
	rst 38h			;6c29	ff		.
	ld a,a			;6c2a	7f		.
	rst 38h			;6c2b	ff		.
	rst 38h			;6c2c	ff		.
	rst 38h			;6c2d	ff		.
	ld a,a			;6c2e	7f		.
	rst 38h			;6c2f	ff		.
	rst 38h			;6c30	ff		.
	rst 38h			;6c31	ff		.
	ld a,c			;6c32	79		y
	nop			;6c33	00		.
	nop			;6c34	00		.
	nop			;6c35	00		.
	add a,e			;6c36	83		.
	nop			;6c37	00		.
	nop			;6c38	00		.
	nop			;6c39	00		.
	add a,(hl)		;6c3a	86		.
	rst 38h			;6c3b	ff		.
	rst 38h			;6c3c	ff		.
	rst 38h			;6c3d	ff		.
	ld a,h			;6c3e	7c		|
	inc b			;6c3f	04		.
	ld bc,00505h		;6c40	01 05 05	. . .
	dec b			;6c43	05		.
	ld b,l			;6c44	45		E
	inc b			;6c45	04		.
	ex af,af'		;6c46	08		.
	ld a,h			;6c47	7c		|
	nop			;6c48	00		.
	nop			;6c49	00		.
	nop			;6c4a	00		.
	ld a,(hl)		;6c4b	7e		~
	nop			;6c4c	00		.
	ld a,(hl)		;6c4d	7e		~
	nop			;6c4e	00		.
	ld a,a			;6c4f	7f		.
	ld a,a			;6c50	7f		.
	ld a,(hl)		;6c51	7e		~
	cp 07fh			;6c52	fe 7f		. .
	ld a,a			;6c54	7f		.
	ld a,(hl)		;6c55	7e		~
	cp 079h			;6c56	fe 79		. y
	ld a,a			;6c58	7f		.
	ld a,(hl)		;6c59	7e		~
	cp 083h			;6c5a	fe 83		. .
	ld a,a			;6c5c	7f		.
	nop			;6c5d	00		.
	cp 086h			;6c5e	fe 86		. .
	nop			;6c60	00		.
	ld a,(hl)		;6c61	7e		~
	nop			;6c62	00		.
	ld a,h			;6c63	7c		|
	nop			;6c64	00		.
	nop			;6c65	00		.
	nop			;6c66	00		.
	inc b			;6c67	04		.
	ld bc,00745h		;6c68	01 45 07	. E .
	ld b,a			;6c6b	47		G
	rlca			;6c6c	07		.
	inc b			;6c6d	04		.
	ex af,af'		;6c6e	08		.
	ld a,0ffh		;6c6f	3e ff		> .
	rst 38h			;6c71	ff		.
	rst 38h			;6c72	ff		.
	ld a,(hl)		;6c73	7e		~
	rst 38h			;6c74	ff		.
	rst 38h			;6c75	ff		.
	rst 38h			;6c76	ff		.
	cp 0ffh			;6c77	fe ff		. .
	rst 38h			;6c79	ff		.
	rst 38h			;6c7a	ff		.
	cp 0ffh			;6c7b	fe ff		. .
	rst 38h			;6c7d	ff		.
	rst 38h			;6c7e	ff		.
	sbc a,(hl)		;6c7f	9e		.
	rst 38h			;6c80	ff		.
	rst 38h			;6c81	ff		.
	rst 38h			;6c82	ff		.
	pop bc			;6c83	c1		.
	nop			;6c84	00		.
	nop			;6c85	00		.
	nop			;6c86	00		.
	ld h,c			;6c87	61		a
	nop			;6c88	00		.
	nop			;6c89	00		.
	nop			;6c8a	00		.
	ld a,0ffh		;6c8b	3e ff		> .
	rst 38h			;6c8d	ff		.
	rst 38h			;6c8e	ff		.
	inc b			;6c8f	04		.
	ld bc,00545h		;6c90	01 45 05	. E .
	dec b			;6c93	05		.
	dec b			;6c94	05		.
	inc b			;6c95	04		.
	ex af,af'		;6c96	08		.
	ld (hl),0ffh		;6c97	36 ff		6 .
	rst 38h			;6c99	ff		.
	ld a,c			;6c9a	79		y
	halt			;6c9b	76		v
	rst 38h			;6c9c	ff		.
	rst 38h			;6c9d	ff		.
	cp c			;6c9e	b9		.
	or 0ffh			;6c9f	f6 ff		. .
	rst 38h			;6ca1	ff		.
	exx			;6ca2	d9		.
	or 0ffh			;6ca3	f6 ff		. .
	rst 38h			;6ca5	ff		.
	jp (hl)			;6ca6	e9		.
	sub (hl)		;6ca7	96		.
	rst 38h			;6ca8	ff		.
	rst 38h			;6ca9	ff		.
	push af			;6caa	f5		.
	ret			;6cab	c9		.
	nop			;6cac	00		.
	nop			;6cad	00		.
	dec bc			;6cae	0b		.
	ld l,c			;6caf	69		i
	nop			;6cb0	00		.
	nop			;6cb1	00		.
	dec b			;6cb2	05		.
	ld (hl),0ffh		;6cb3	36 ff		6 .
	rst 38h			;6cb5	ff		.
	cp 004h			;6cb6	fe 04		. .
	ld bc,00545h		;6cb8	01 45 05	. E .
	dec b			;6cbb	05		.
	dec b			;6cbc	05		.
l6cbdh:
	in a,(06ch)		;6cbd	db 6c		. l
	adc a,a			;6cbf	8f		.
	ld l,l			;6cc0	6d		m
	ld b,e			;6cc1	43		C
	ld l,(hl)		;6cc2	6e		n
	rst 30h			;6cc3	f7		.
	ld l,(hl)		;6cc4	6e		n
	xor e			;6cc5	ab		.
	ld l,a			;6cc6	6f		o
	ld e,a			;6cc7	5f		_
	ld (hl),b		;6cc8	70		p
	inc de			;6cc9	13		.
	ld (hl),c		;6cca	71		q
	rst 0			;6ccb	c7		.
	ld (hl),c		;6ccc	71		q
	ld a,e			;6ccd	7b		{
	ld (hl),d		;6cce	72		r
	cpl			;6ccf	2f		/
	ld (hl),e		;6cd0	73		s
	ex (sp),hl		;6cd1	e3		.
	ld (hl),e		;6cd2	73		s
	sub a			;6cd3	97		.
	ld (hl),h		;6cd4	74		t
	ld c,e			;6cd5	4b		K
	ld (hl),l		;6cd6	75		u
	rst 38h			;6cd7	ff		.
	ld (hl),l		;6cd8	75		u
	or e			;6cd9	b3		.
	halt			;6cda	76		v
	ret nz			;6cdb	c0		.
	ret nz			;6cdc	c0		.
	ret nz			;6cdd	c0		.
	rlca			;6cde	07		.
	rlca			;6cdf	07		.
	rlca			;6ce0	07		.
	rlca			;6ce1	07		.
	rlca			;6ce2	07		.
	rlca			;6ce3	07		.
	rlca			;6ce4	07		.
	rlca			;6ce5	07		.
	rlca			;6ce6	07		.
	ret nz			;6ce7	c0		.
	ret nz			;6ce8	c0		.
	ret nz			;6ce9	c0		.
	ret nz			;6cea	c0		.
	ret nz			;6ceb	c0		.
	inc de			;6cec	13		.
	ld (de),a		;6ced	12		.
	ld (de),a		;6cee	12		.
	ld (de),a		;6cef	12		.
	ld (de),a		;6cf0	12		.
	ld (de),a		;6cf1	12		.
	ld (de),a		;6cf2	12		.
	ld (de),a		;6cf3	12		.
	ld (de),a		;6cf4	12		.
	ld (de),a		;6cf5	12		.
	inc de			;6cf6	13		.
	ret nz			;6cf7	c0		.
	ret nz			;6cf8	c0		.
	ret nz			;6cf9	c0		.
	inc de			;6cfa	13		.
	inc de			;6cfb	13		.
	inc de			;6cfc	13		.
	ld (de),a		;6cfd	12		.
	ld (de),a		;6cfe	12		.
	ld (de),a		;6cff	12		.
	ld (de),a		;6d00	12		.
	ld (de),a		;6d01	12		.
	ld (de),a		;6d02	12		.
	ld (de),a		;6d03	12		.
	inc de			;6d04	13		.
	inc de			;6d05	13		.
	inc de			;6d06	13		.
	ret nz			;6d07	c0		.
	inc de			;6d08	13		.
	inc de			;6d09	13		.
	rlca			;6d0a	07		.
	inc de			;6d0b	13		.
	inc de			;6d0c	13		.
	inc d			;6d0d	14		.
	inc d			;6d0e	14		.
	inc d			;6d0f	14		.
	inc d			;6d10	14		.
	inc d			;6d11	14		.
	inc de			;6d12	13		.
	inc de			;6d13	13		.
	rlca			;6d14	07		.
	inc de			;6d15	13		.
	inc de			;6d16	13		.
	ld (de),a		;6d17	12		.
	inc de			;6d18	13		.
	inc de			;6d19	13		.
	inc de			;6d1a	13		.
	dec d			;6d1b	15		.
	dec d			;6d1c	15		.
	dec d			;6d1d	15		.
	dec d			;6d1e	15		.
	dec d			;6d1f	15		.
	dec d			;6d20	15		.
	dec d			;6d21	15		.
	inc de			;6d22	13		.
	inc de			;6d23	13		.
	inc de			;6d24	13		.
	ld (de),a		;6d25	12		.
	ld (de),a		;6d26	12		.
	ld (de),a		;6d27	12		.
	inc de			;6d28	13		.
	dec d			;6d29	15		.
	dec d			;6d2a	15		.
	dec d			;6d2b	15		.
	dec d			;6d2c	15		.
	dec d			;6d2d	15		.
	dec d			;6d2e	15		.
	dec d			;6d2f	15		.
	dec d			;6d30	15		.
	dec d			;6d31	15		.
	inc de			;6d32	13		.
	ld (de),a		;6d33	12		.
	ld (de),a		;6d34	12		.
	ret nz			;6d35	c0		.
	ret nz			;6d36	c0		.
	ret nz			;6d37	c0		.
	ret nz			;6d38	c0		.
	ret nz			;6d39	c0		.
	ret nz			;6d3a	c0		.
	ret nz			;6d3b	c0		.
	ret nz			;6d3c	c0		.
	ret nz			;6d3d	c0		.
	ret nz			;6d3e	c0		.
	ret nz			;6d3f	c0		.
	ret nz			;6d40	c0		.
	ret nz			;6d41	c0		.
	ret nz			;6d42	c0		.
	ret nz			;6d43	c0		.
	ret nz			;6d44	c0		.
	ret nz			;6d45	c0		.
	ret nz			;6d46	c0		.
	ret nz			;6d47	c0		.
	ret nz			;6d48	c0		.
	ret nz			;6d49	c0		.
	ret nz			;6d4a	c0		.
	ret nz			;6d4b	c0		.
	ret nz			;6d4c	c0		.
	ret nz			;6d4d	c0		.
	ret nz			;6d4e	c0		.
	ret nz			;6d4f	c0		.
	ret nz			;6d50	c0		.
	ret nz			;6d51	c0		.
	ret nz			;6d52	c0		.
	ret nz			;6d53	c0		.
	ret nz			;6d54	c0		.
	ret nz			;6d55	c0		.
	ret nz			;6d56	c0		.
	ret nz			;6d57	c0		.
	ret nz			;6d58	c0		.
	ret nz			;6d59	c0		.
	ret nz			;6d5a	c0		.
	ret nz			;6d5b	c0		.
	ret nz			;6d5c	c0		.
	ret nz			;6d5d	c0		.
	ret nz			;6d5e	c0		.
	ret nz			;6d5f	c0		.
	ret nz			;6d60	c0		.
	ret nz			;6d61	c0		.
	ret nz			;6d62	c0		.
	ret nz			;6d63	c0		.
	ret nz			;6d64	c0		.
	ret nz			;6d65	c0		.
	ret nz			;6d66	c0		.
	ret nz			;6d67	c0		.
	ret nz			;6d68	c0		.
	ret nz			;6d69	c0		.
	ret nz			;6d6a	c0		.
	ret nz			;6d6b	c0		.
	ret nz			;6d6c	c0		.
	ret nz			;6d6d	c0		.
	ret nz			;6d6e	c0		.
	ret nz			;6d6f	c0		.
	ret nz			;6d70	c0		.
	ret nz			;6d71	c0		.
	ret nz			;6d72	c0		.
	ret nz			;6d73	c0		.
	ret nz			;6d74	c0		.
	ret nz			;6d75	c0		.
	ret nz			;6d76	c0		.
	ret nz			;6d77	c0		.
	ret nz			;6d78	c0		.
	ret nz			;6d79	c0		.
	ret nz			;6d7a	c0		.
	ret nz			;6d7b	c0		.
	ret nz			;6d7c	c0		.
	ret nz			;6d7d	c0		.
	ret nz			;6d7e	c0		.
	ret nz			;6d7f	c0		.
	ret nz			;6d80	c0		.
	ret nz			;6d81	c0		.
	ret nz			;6d82	c0		.
	ret nz			;6d83	c0		.
	ret nz			;6d84	c0		.
	ret nz			;6d85	c0		.
	ret nz			;6d86	c0		.
	ret nz			;6d87	c0		.
	ret nz			;6d88	c0		.
	ret nz			;6d89	c0		.
	ret nz			;6d8a	c0		.
	ret nz			;6d8b	c0		.
	ret nz			;6d8c	c0		.
	ret nz			;6d8d	c0		.
	ret nz			;6d8e	c0		.
	ret nz			;6d8f	c0		.
	ret nz			;6d90	c0		.
	ld b,006h		;6d91	06 06		. .
	ld b,006h		;6d93	06 06		. .
	ld b,006h		;6d95	06 06		. .
	ld b,006h		;6d97	06 06		. .
	ld b,006h		;6d99	06 06		. .
	ld b,0c0h		;6d9b	06 c0		. .
	ret nz			;6d9d	c0		.
	ret nz			;6d9e	c0		.
	ld b,006h		;6d9f	06 06		. .
	dec d			;6da1	15		.
	inc de			;6da2	13		.
	inc de			;6da3	13		.
	ret nz			;6da4	c0		.
	ret nz			;6da5	c0		.
	ret nz			;6da6	c0		.
	inc de			;6da7	13		.
	inc de			;6da8	13		.
	dec d			;6da9	15		.
	ld b,006h		;6daa	06 06		. .
	ret nz			;6dac	c0		.
	ld b,006h		;6dad	06 06		. .
	dec d			;6daf	15		.
	dec d			;6db0	15		.
	inc de			;6db1	13		.
	inc de			;6db2	13		.
	ret nz			;6db3	c0		.
	ret nz			;6db4	c0		.
	ret nz			;6db5	c0		.
	inc de			;6db6	13		.
	inc de			;6db7	13		.
	dec d			;6db8	15		.
	dec d			;6db9	15		.
	ld b,006h		;6dba	06 06		. .
	ld b,012h		;6dbc	06 12		. .
	dec d			;6dbe	15		.
	dec d			;6dbf	15		.
	inc de			;6dc0	13		.
	inc de			;6dc1	13		.
	ret nz			;6dc2	c0		.
	ret nz			;6dc3	c0		.
	ret nz			;6dc4	c0		.
	inc de			;6dc5	13		.
	inc de			;6dc6	13		.
	dec d			;6dc7	15		.
	dec d			;6dc8	15		.
	ld (de),a		;6dc9	12		.
	ld b,012h		;6dca	06 12		. .
	ld (de),a		;6dcc	12		.
	dec d			;6dcd	15		.
	dec d			;6dce	15		.
	inc de			;6dcf	13		.
	inc de			;6dd0	13		.
	ret nz			;6dd1	c0		.
	ret nz			;6dd2	c0		.
	ret nz			;6dd3	c0		.
	inc de			;6dd4	13		.
	inc de			;6dd5	13		.
	dec d			;6dd6	15		.
	dec d			;6dd7	15		.
	ld (de),a		;6dd8	12		.
	ld (de),a		;6dd9	12		.
	ld (de),a		;6dda	12		.
	ld (de),a		;6ddb	12		.
	dec d			;6ddc	15		.
	ld b,006h		;6ddd	06 06		. .
	ld b,006h		;6ddf	06 06		. .
	ld b,006h		;6de1	06 06		. .
	ld b,006h		;6de3	06 06		. .
	ld b,015h		;6de5	06 15		. .
	ld (de),a		;6de7	12		.
	ld (de),a		;6de8	12		.
	ld (de),a		;6de9	12		.
	ld (de),a		;6dea	12		.
	ld b,006h		;6deb	06 06		. .
	ret nz			;6ded	c0		.
	ret nz			;6dee	c0		.
	ret nz			;6def	c0		.
	ret nz			;6df0	c0		.
	ret nz			;6df1	c0		.
	ret nz			;6df2	c0		.
	ret nz			;6df3	c0		.
	ld b,006h		;6df4	06 06		. .
	ld (de),a		;6df6	12		.
	ld (de),a		;6df7	12		.
	ld (de),a		;6df8	12		.
	ld b,006h		;6df9	06 06		. .
	ret nz			;6dfb	c0		.
	ret nz			;6dfc	c0		.
	ret nz			;6dfd	c0		.
	ret nz			;6dfe	c0		.
	ret nz			;6dff	c0		.
	ret nz			;6e00	c0		.
	ret nz			;6e01	c0		.
	ret nz			;6e02	c0		.
	ret nz			;6e03	c0		.
	ld b,006h		;6e04	06 06		. .
	ld (de),a		;6e06	12		.
	ld b,006h		;6e07	06 06		. .
	ret nz			;6e09	c0		.
	ret nz			;6e0a	c0		.
	ret nz			;6e0b	c0		.
	ret nz			;6e0c	c0		.
	ret nz			;6e0d	c0		.
	ret nz			;6e0e	c0		.
	ret nz			;6e0f	c0		.
	ret nz			;6e10	c0		.
	ret nz			;6e11	c0		.
	ret nz			;6e12	c0		.
	ret nz			;6e13	c0		.
	ld b,006h		;6e14	06 06		. .
	ld b,0c0h		;6e16	06 c0		. .
	ret nz			;6e18	c0		.
	ret nz			;6e19	c0		.
	ret nz			;6e1a	c0		.
	ret nz			;6e1b	c0		.
	ret nz			;6e1c	c0		.
	ret nz			;6e1d	c0		.
	ret nz			;6e1e	c0		.
	ret nz			;6e1f	c0		.
	ret nz			;6e20	c0		.
	ret nz			;6e21	c0		.
	ret nz			;6e22	c0		.
	ret nz			;6e23	c0		.
	ld b,0c0h		;6e24	06 c0		. .
	ret nz			;6e26	c0		.
	ret nz			;6e27	c0		.
	ret nz			;6e28	c0		.
	ret nz			;6e29	c0		.
	ret nz			;6e2a	c0		.
	ret nz			;6e2b	c0		.
	ret nz			;6e2c	c0		.
	ret nz			;6e2d	c0		.
	ret nz			;6e2e	c0		.
	ret nz			;6e2f	c0		.
	ret nz			;6e30	c0		.
	ret nz			;6e31	c0		.
	ret nz			;6e32	c0		.
	ret nz			;6e33	c0		.
	ret nz			;6e34	c0		.
	ret nz			;6e35	c0		.
	ret nz			;6e36	c0		.
	ret nz			;6e37	c0		.
	ret nz			;6e38	c0		.
	ret nz			;6e39	c0		.
	ret nz			;6e3a	c0		.
	ret nz			;6e3b	c0		.
	ret nz			;6e3c	c0		.
	ret nz			;6e3d	c0		.
	ret nz			;6e3e	c0		.
	ret nz			;6e3f	c0		.
	ret nz			;6e40	c0		.
	ret nz			;6e41	c0		.
	ret nz			;6e42	c0		.
	ret nz			;6e43	c0		.
	ret nz			;6e44	c0		.
	ret nz			;6e45	c0		.
	ret nz			;6e46	c0		.
	ret nz			;6e47	c0		.
	ret nz			;6e48	c0		.
	ret nz			;6e49	c0		.
	ret nz			;6e4a	c0		.
	ret nz			;6e4b	c0		.
	ret nz			;6e4c	c0		.
	ret nz			;6e4d	c0		.
	ret nz			;6e4e	c0		.
	ret nz			;6e4f	c0		.
	ret nz			;6e50	c0		.
	ret nz			;6e51	c0		.
	ld b,006h		;6e52	06 06		. .
	ld b,006h		;6e54	06 06		. .
	ld b,02ch		;6e56	06 2c		. ,
	ret nz			;6e58	c0		.
	ret nz			;6e59	c0		.
	ret nz			;6e5a	c0		.
	inc l			;6e5b	2c		,
	ld b,006h		;6e5c	06 06		. .
	ld b,006h		;6e5e	06 06		. .
	ld b,0c0h		;6e60	06 c0		. .
	ret nz			;6e62	c0		.
	ret nz			;6e63	c0		.
	ret nz			;6e64	c0		.
	ret nz			;6e65	c0		.
	inc l			;6e66	2c		,
	ret nz			;6e67	c0		.
	ret nz			;6e68	c0		.
	ret nz			;6e69	c0		.
	inc l			;6e6a	2c		,
	ret nz			;6e6b	c0		.
	ret nz			;6e6c	c0		.
	ret nz			;6e6d	c0		.
	ret nz			;6e6e	c0		.
	ret nz			;6e6f	c0		.
	ret nz			;6e70	c0		.
	ret nz			;6e71	c0		.
	ret nz			;6e72	c0		.
	ret nz			;6e73	c0		.
	ret nz			;6e74	c0		.
	inc l			;6e75	2c		,
	ret nz			;6e76	c0		.
	ret nz			;6e77	c0		.
	ret nz			;6e78	c0		.
	inc l			;6e79	2c		,
	ret nz			;6e7a	c0		.
	ret nz			;6e7b	c0		.
	ret nz			;6e7c	c0		.
	ret nz			;6e7d	c0		.
	ret nz			;6e7e	c0		.
	ret nz			;6e7f	c0		.
	inc l			;6e80	2c		,
	inc l			;6e81	2c		,
	inc l			;6e82	2c		,
	inc l			;6e83	2c		,
	inc l			;6e84	2c		,
	ret nz			;6e85	c0		.
	inc de			;6e86	13		.
	ret nz			;6e87	c0		.
	inc l			;6e88	2c		,
	inc l			;6e89	2c		,
	inc l			;6e8a	2c		,
	inc l			;6e8b	2c		,
	inc l			;6e8c	2c		,
	ret nz			;6e8d	c0		.
	ret nz			;6e8e	c0		.
	inc l			;6e8f	2c		,
	dec d			;6e90	15		.
	ret nz			;6e91	c0		.
	ret nz			;6e92	c0		.
	ret nz			;6e93	c0		.
	inc de			;6e94	13		.
	inc de			;6e95	13		.
	inc de			;6e96	13		.
	ret nz			;6e97	c0		.
	ret nz			;6e98	c0		.
	ret nz			;6e99	c0		.
	dec d			;6e9a	15		.
	inc l			;6e9b	2c		,
	ret nz			;6e9c	c0		.
	ret nz			;6e9d	c0		.
	inc l			;6e9e	2c		,
	dec d			;6e9f	15		.
	ret nz			;6ea0	c0		.
	ret nz			;6ea1	c0		.
	ret nz			;6ea2	c0		.
	inc de			;6ea3	13		.
	inc de			;6ea4	13		.
	inc de			;6ea5	13		.
	ret nz			;6ea6	c0		.
	ret nz			;6ea7	c0		.
	ret nz			;6ea8	c0		.
	dec d			;6ea9	15		.
	inc l			;6eaa	2c		,
	ret nz			;6eab	c0		.
	ret nz			;6eac	c0		.
	inc l			;6ead	2c		,
	inc l			;6eae	2c		,
	inc l			;6eaf	2c		,
	inc l			;6eb0	2c		,
	inc l			;6eb1	2c		,
	ret nz			;6eb2	c0		.
	inc de			;6eb3	13		.
	ret nz			;6eb4	c0		.
	inc l			;6eb5	2c		,
	inc l			;6eb6	2c		,
	inc l			;6eb7	2c		,
	inc l			;6eb8	2c		,
	inc l			;6eb9	2c		,
	ret nz			;6eba	c0		.
	ret nz			;6ebb	c0		.
	ret nz			;6ebc	c0		.
	ret nz			;6ebd	c0		.
	ret nz			;6ebe	c0		.
	ld (de),a		;6ebf	12		.
	inc l			;6ec0	2c		,
	ret nz			;6ec1	c0		.
	ret nz			;6ec2	c0		.
	ret nz			;6ec3	c0		.
	inc l			;6ec4	2c		,
	ld (de),a		;6ec5	12		.
	ret nz			;6ec6	c0		.
	ret nz			;6ec7	c0		.
	ret nz			;6ec8	c0		.
	ret nz			;6ec9	c0		.
	ret nz			;6eca	c0		.
	ret nz			;6ecb	c0		.
	ret nz			;6ecc	c0		.
	ret nz			;6ecd	c0		.
	ld (de),a		;6ece	12		.
	inc l			;6ecf	2c		,
	ret nz			;6ed0	c0		.
	ret nz			;6ed1	c0		.
	ret nz			;6ed2	c0		.
	inc l			;6ed3	2c		,
	ld (de),a		;6ed4	12		.
	ret nz			;6ed5	c0		.
	ret nz			;6ed6	c0		.
	ret nz			;6ed7	c0		.
	ret nz			;6ed8	c0		.
	inc l			;6ed9	2c		,
	inc l			;6eda	2c		,
	inc l			;6edb	2c		,
	inc l			;6edc	2c		,
	inc l			;6edd	2c		,
	inc l			;6ede	2c		,
	ret nz			;6edf	c0		.
	ret nz			;6ee0	c0		.
	ret nz			;6ee1	c0		.
	inc l			;6ee2	2c		,
	inc l			;6ee3	2c		,
	inc l			;6ee4	2c		,
	inc l			;6ee5	2c		,
	inc l			;6ee6	2c		,
	inc l			;6ee7	2c		,
	ret nz			;6ee8	c0		.
	ret nz			;6ee9	c0		.
	ret nz			;6eea	c0		.
	ret nz			;6eeb	c0		.
	ret nz			;6eec	c0		.
	ret nz			;6eed	c0		.
	ret nz			;6eee	c0		.
	ret nz			;6eef	c0		.
	ret nz			;6ef0	c0		.
	ret nz			;6ef1	c0		.
	ret nz			;6ef2	c0		.
	ret nz			;6ef3	c0		.
	ret nz			;6ef4	c0		.
	ret nz			;6ef5	c0		.
	ret nz			;6ef6	c0		.
	ret nz			;6ef7	c0		.
	ret nz			;6ef8	c0		.
	inc d			;6ef9	14		.
	inc d			;6efa	14		.
	inc de			;6efb	13		.
	inc de			;6efc	13		.
	ret nz			;6efd	c0		.
	ret nz			;6efe	c0		.
	ret nz			;6eff	c0		.
	inc de			;6f00	13		.
	inc de			;6f01	13		.
	inc d			;6f02	14		.
	inc d			;6f03	14		.
	ret nz			;6f04	c0		.
	ret nz			;6f05	c0		.
	ret nz			;6f06	c0		.
	inc d			;6f07	14		.
	inc d			;6f08	14		.
	inc de			;6f09	13		.
	inc de			;6f0a	13		.
	ld b,011h		;6f0b	06 11		. .
	ret nz			;6f0d	c0		.
	ld de,01306h		;6f0e	11 06 13	. . .
	inc de			;6f11	13		.
	inc d			;6f12	14		.
	inc d			;6f13	14		.
	ret nz			;6f14	c0		.
	ret nz			;6f15	c0		.
	inc d			;6f16	14		.
	inc de			;6f17	13		.
	inc de			;6f18	13		.
	ld b,011h		;6f19	06 11		. .
	ld de,011c0h		;6f1b	11 c0 11	. . .
	ld de,01306h		;6f1e	11 06 13	. . .
	inc de			;6f21	13		.
	inc d			;6f22	14		.
	ret nz			;6f23	c0		.
	ret nz			;6f24	c0		.
	inc de			;6f25	13		.
	inc de			;6f26	13		.
	ld b,011h		;6f27	06 11		. .
	ld de,lc015h		;6f29	11 15 c0	. . .
	dec d			;6f2c	15		.
	ld de,00611h		;6f2d	11 11 06	. . .
	inc de			;6f30	13		.
	inc de			;6f31	13		.
	ret nz			;6f32	c0		.
	ret nz			;6f33	c0		.
	inc de			;6f34	13		.
	ld b,011h		;6f35	06 11		. .
	ld de,01215h		;6f37	11 15 12	. . .
	ret nz			;6f3a	c0		.
	ld (de),a		;6f3b	12		.
	dec d			;6f3c	15		.
	ld de,00611h		;6f3d	11 11 06	. . .
	inc de			;6f40	13		.
	ret nz			;6f41	c0		.
	ret nz			;6f42	c0		.
	ld b,011h		;6f43	06 11		. .
	ret nz			;6f45	c0		.
	ret nz			;6f46	c0		.
	ld (de),a		;6f47	12		.
	ld (de),a		;6f48	12		.
	ret nz			;6f49	c0		.
	ld (de),a		;6f4a	12		.
	ld (de),a		;6f4b	12		.
	ret nz			;6f4c	c0		.
	ret nz			;6f4d	c0		.
	ld de,lc006h		;6f4e	11 06 c0	. . .
	ret nz			;6f51	c0		.
	ld de,lc011h		;6f52	11 11 c0	. . .
	ret nz			;6f55	c0		.
	ld (de),a		;6f56	12		.
	ld b,0c0h		;6f57	06 c0		. .
	ld b,012h		;6f59	06 12		. .
	ret nz			;6f5b	c0		.
	ret nz			;6f5c	c0		.
	ld de,lc011h		;6f5d	11 11 c0	. . .
	ret nz			;6f60	c0		.
	ld de,01215h		;6f61	11 15 12	. . .
	ld (de),a		;6f64	12		.
	ld b,013h		;6f65	06 13		. .
	ret nz			;6f67	c0		.
	inc de			;6f68	13		.
	ld b,012h		;6f69	06 12		. .
	ld (de),a		;6f6b	12		.
	dec d			;6f6c	15		.
	ld de,sub_c0c0h		;6f6d	11 c0 c0	. . .
	dec d			;6f70	15		.
	ld (de),a		;6f71	12		.
	ld (de),a		;6f72	12		.
	ld b,013h		;6f73	06 13		. .
	inc de			;6f75	13		.
	ret nz			;6f76	c0		.
	inc de			;6f77	13		.
	inc de			;6f78	13		.
	ld b,012h		;6f79	06 12		. .
	ld (de),a		;6f7b	12		.
	dec d			;6f7c	15		.
	ret nz			;6f7d	c0		.
	ret nz			;6f7e	c0		.
	ld (de),a		;6f7f	12		.
	ld (de),a		;6f80	12		.
	ld b,013h		;6f81	06 13		. .
	inc de			;6f83	13		.
	inc d			;6f84	14		.
	ret nz			;6f85	c0		.
	inc d			;6f86	14		.
	inc de			;6f87	13		.
	inc de			;6f88	13		.
	ld b,012h		;6f89	06 12		. .
	ld (de),a		;6f8b	12		.
	ret nz			;6f8c	c0		.
	ret nz			;6f8d	c0		.
	ld (de),a		;6f8e	12		.
	ld b,013h		;6f8f	06 13		. .
	inc de			;6f91	13		.
	inc d			;6f92	14		.
	inc d			;6f93	14		.
	ret nz			;6f94	c0		.
	inc d			;6f95	14		.
	inc d			;6f96	14		.
	inc de			;6f97	13		.
	inc de			;6f98	13		.
	ld b,012h		;6f99	06 12		. .
	ret nz			;6f9b	c0		.
	ret nz			;6f9c	c0		.
	ret nz			;6f9d	c0		.
	inc de			;6f9e	13		.
	inc de			;6f9f	13		.
	inc d			;6fa0	14		.
	inc d			;6fa1	14		.
	ret nz			;6fa2	c0		.
	ret nz			;6fa3	c0		.
	ret nz			;6fa4	c0		.
	inc d			;6fa5	14		.
	inc d			;6fa6	14		.
	inc de			;6fa7	13		.
	inc de			;6fa8	13		.
	ret nz			;6fa9	c0		.
	ret nz			;6faa	c0		.
	ld l,02eh		;6fab	2e 2e		. .
	ld l,02eh		;6fad	2e 2e		. .
	ret nz			;6faf	c0		.
	ret nz			;6fb0	c0		.
	ld l,02eh		;6fb1	2e 2e		. .
	ld l,0c0h		;6fb3	2e c0		. .
	ret nz			;6fb5	c0		.
	ld l,02eh		;6fb6	2e 2e		. .
	ld l,02eh		;6fb8	2e 2e		. .
	ld l,02eh		;6fba	2e 2e		. .
	ld l,02eh		;6fbc	2e 2e		. .
	ld l,02eh		;6fbe	2e 2e		. .
	ld l,02eh		;6fc0	2e 2e		. .
	ld l,02eh		;6fc2	2e 2e		. .
	ld l,02eh		;6fc4	2e 2e		. .
	ld l,02eh		;6fc6	2e 2e		. .
	ld l,0c0h		;6fc8	2e c0		. .
	ret nz			;6fca	c0		.
	ret nz			;6fcb	c0		.
	ret nz			;6fcc	c0		.
	ld l,0c0h		;6fcd	2e c0		. .
	ret nz			;6fcf	c0		.
	ret nz			;6fd0	c0		.
	ret nz			;6fd1	c0		.
	ret nz			;6fd2	c0		.
	ld l,0c0h		;6fd3	2e c0		. .
	ret nz			;6fd5	c0		.
	ret nz			;6fd6	c0		.
	ld l,0c0h		;6fd7	2e c0		. .
	ret nz			;6fd9	c0		.
	ret nz			;6fda	c0		.
	ret nz			;6fdb	c0		.
	ld l,0c0h		;6fdc	2e c0		. .
	ret nz			;6fde	c0		.
	ret nz			;6fdf	c0		.
	ret nz			;6fe0	c0		.
	ret nz			;6fe1	c0		.
	ld l,0c0h		;6fe2	2e c0		. .
	ret nz			;6fe4	c0		.
	ret nz			;6fe5	c0		.
	ld l,0c0h		;6fe6	2e c0		. .
	ld l,011h		;6fe8	2e 11		. .
	ret nz			;6fea	c0		.
	ld l,0c0h		;6feb	2e c0		. .
	ld (de),a		;6fed	12		.
	ld l,014h		;6fee	2e 14		. .
	ret nz			;6ff0	c0		.
	ld l,0c0h		;6ff1	2e c0		. .
	ld l,0c0h		;6ff3	2e c0		. .
	ld l,0c0h		;6ff5	2e c0		. .
	ld l,0c0h		;6ff7	2e c0		. .
	ret nz			;6ff9	c0		.
	ld l,0c0h		;6ffa	2e c0		. .
	ret nz			;6ffc	c0		.
	ld l,0c0h		;6ffd	2e c0		. .
	ret nz			;6fff	c0		.
	ld l,015h		;7000	2e 15		. .
	ld l,015h		;7002	2e 15		. .
	ld l,0c0h		;7004	2e c0		. .
	ld l,0c0h		;7006	2e c0		. .
	ret nz			;7008	c0		.
	ld l,0c0h		;7009	2e c0		. .
	ret nz			;700b	c0		.
	ld l,0c0h		;700c	2e c0		. .
	ret nz			;700e	c0		.
	ld l,0c0h		;700f	2e c0		. .
	ld l,0c0h		;7011	2e c0		. .
	ld l,0c0h		;7013	2e c0		. .
	ld l,0c0h		;7015	2e c0		. .
	ld de,0122eh		;7017	11 2e 12	. . .
	ret nz			;701a	c0		.
	ld l,0c0h		;701b	2e c0		. .
	inc d			;701d	14		.
	ld l,0c0h		;701e	2e c0		. .
	ld l,0c0h		;7020	2e c0		. .
	ld l,0c0h		;7022	2e c0		. .
	ld l,0c0h		;7024	2e c0		. .
	ret nz			;7026	c0		.
	ret nz			;7027	c0		.
	ret nz			;7028	c0		.
	ret nz			;7029	c0		.
	ld l,0c0h		;702a	2e c0		. .
	ret nz			;702c	c0		.
	ret nz			;702d	c0		.
	ret nz			;702e	c0		.
	ld l,0c0h		;702f	2e c0		. .
	ld l,0c0h		;7031	2e c0		. .
	ld l,0c0h		;7033	2e c0		. .
	ret nz			;7035	c0		.
	ret nz			;7036	c0		.
	ret nz			;7037	c0		.
	ret nz			;7038	c0		.
	ld l,0c0h		;7039	2e c0		. .
	ret nz			;703b	c0		.
	ret nz			;703c	c0		.
	ret nz			;703d	c0		.
	ld l,015h		;703e	2e 15		. .
	ld l,0c0h		;7040	2e c0		. .
	ld l,02eh		;7042	2e 2e		. .
	ld l,02eh		;7044	2e 2e		. .
	ld l,02eh		;7046	2e 2e		. .
	ld l,02eh		;7048	2e 2e		. .
	ld l,02eh		;704a	2e 2e		. .
	ld l,02eh		;704c	2e 2e		. .
	ld l,02eh		;704e	2e 2e		. .
	ret nz			;7050	c0		.
	ld l,02eh		;7051	2e 2e		. .
	ld l,02eh		;7053	2e 2e		. .
	ld l,02eh		;7055	2e 2e		. .
	ld l,02eh		;7057	2e 2e		. .
	ld l,02eh		;7059	2e 2e		. .
	ld l,02eh		;705b	2e 2e		. .
	ld l,02eh		;705d	2e 2e		. .
	ret nz			;705f	c0		.
	ret nz			;7060	c0		.
	rlca			;7061	07		.
	rlca			;7062	07		.
	ret nz			;7063	c0		.
	ret nz			;7064	c0		.
	ret nz			;7065	c0		.
	ret nz			;7066	c0		.
	ret nz			;7067	c0		.
	ret nz			;7068	c0		.
	ret nz			;7069	c0		.
	rlca			;706a	07		.
	rlca			;706b	07		.
	ret nz			;706c	c0		.
	ret nz			;706d	c0		.
	ret nz			;706e	c0		.
	rlca			;706f	07		.
	rlca			;7070	07		.
	rlca			;7071	07		.
	rlca			;7072	07		.
	rlca			;7073	07		.
	ret nz			;7074	c0		.
	ret nz			;7075	c0		.
	ret nz			;7076	c0		.
	rlca			;7077	07		.
	rlca			;7078	07		.
	rlca			;7079	07		.
	rlca			;707a	07		.
	rlca			;707b	07		.
	ret nz			;707c	c0		.
	ret nz			;707d	c0		.
	rlca			;707e	07		.
	ld b,006h		;707f	06 06		. .
	rlca			;7081	07		.
	rlca			;7082	07		.
	rlca			;7083	07		.
	ret nz			;7084	c0		.
	rlca			;7085	07		.
	rlca			;7086	07		.
	rlca			;7087	07		.
	ld b,006h		;7088	06 06		. .
	rlca			;708a	07		.
	ret nz			;708b	c0		.
	ret nz			;708c	c0		.
	rlca			;708d	07		.
	ld b,015h		;708e	06 15		. .
	ld b,007h		;7090	06 07		. .
	rlca			;7092	07		.
	ret nz			;7093	c0		.
	rlca			;7094	07		.
	rlca			;7095	07		.
	ld b,015h		;7096	06 15		. .
	ld b,007h		;7098	06 07		. .
	ret nz			;709a	c0		.
	ret nz			;709b	c0		.
	rlca			;709c	07		.
	rlca			;709d	07		.
	ld b,006h		;709e	06 06		. .
	rlca			;70a0	07		.
	rlca			;70a1	07		.
	rlca			;70a2	07		.
	rlca			;70a3	07		.
	rlca			;70a4	07		.
	ld b,006h		;70a5	06 06		. .
	rlca			;70a7	07		.
	rlca			;70a8	07		.
	ret nz			;70a9	c0		.
	ret nz			;70aa	c0		.
	ret nz			;70ab	c0		.
	rlca			;70ac	07		.
	rlca			;70ad	07		.
	rlca			;70ae	07		.
	rlca			;70af	07		.
	rlca			;70b0	07		.
	rlca			;70b1	07		.
	rlca			;70b2	07		.
	rlca			;70b3	07		.
	rlca			;70b4	07		.
	rlca			;70b5	07		.
	rlca			;70b6	07		.
	ret nz			;70b7	c0		.
	ret nz			;70b8	c0		.
	ret nz			;70b9	c0		.
	ret nz			;70ba	c0		.
	ret nz			;70bb	c0		.
	rlca			;70bc	07		.
	rlca			;70bd	07		.
	rlca			;70be	07		.
	rlca			;70bf	07		.
	rlca			;70c0	07		.
	rlca			;70c1	07		.
	rlca			;70c2	07		.
	rlca			;70c3	07		.
	rlca			;70c4	07		.
	ret nz			;70c5	c0		.
	ret nz			;70c6	c0		.
	ret nz			;70c7	c0		.
	ret nz			;70c8	c0		.
	ret nz			;70c9	c0		.
	ret nz			;70ca	c0		.
	ret nz			;70cb	c0		.
	rlca			;70cc	07		.
	rlca			;70cd	07		.
	dec d			;70ce	15		.
	rlca			;70cf	07		.
	dec d			;70d0	15		.
	rlca			;70d1	07		.
	rlca			;70d2	07		.
	ret nz			;70d3	c0		.
	ret nz			;70d4	c0		.
	ret nz			;70d5	c0		.
	ret nz			;70d6	c0		.
	ret nz			;70d7	c0		.
	ret nz			;70d8	c0		.
	rlca			;70d9	07		.
	rlca			;70da	07		.
	rlca			;70db	07		.
	rlca			;70dc	07		.
	dec d			;70dd	15		.
	ret nz			;70de	c0		.
	dec d			;70df	15		.
	rlca			;70e0	07		.
	rlca			;70e1	07		.
	rlca			;70e2	07		.
	rlca			;70e3	07		.
	ret nz			;70e4	c0		.
	ret nz			;70e5	c0		.
	ret nz			;70e6	c0		.
	rlca			;70e7	07		.
	rlca			;70e8	07		.
	rlca			;70e9	07		.
	rlca			;70ea	07		.
	ret nz			;70eb	c0		.
	dec d			;70ec	15		.
	ret nz			;70ed	c0		.
	dec d			;70ee	15		.
	ret nz			;70ef	c0		.
	rlca			;70f0	07		.
	rlca			;70f1	07		.
	rlca			;70f2	07		.
	rlca			;70f3	07		.
	ret nz			;70f4	c0		.
	rlca			;70f5	07		.
	rlca			;70f6	07		.
	rlca			;70f7	07		.
l70f8h:
	ret nz			;70f8	c0		.
	ret nz			;70f9	c0		.
	ret nz			;70fa	c0		.
	ret nz			;70fb	c0		.
	ret nz			;70fc	c0		.
	ret nz			;70fd	c0		.
	ret nz			;70fe	c0		.
	ret nz			;70ff	c0		.
	ret nz			;7100	c0		.
	rlca			;7101	07		.
	rlca			;7102	07		.
	rlca			;7103	07		.
	rlca			;7104	07		.
	rlca			;7105	07		.
	ret nz			;7106	c0		.
	ret nz			;7107	c0		.
	ret nz			;7108	c0		.
	ret nz			;7109	c0		.
	ret nz			;710a	c0		.
	ret nz			;710b	c0		.
	ret nz			;710c	c0		.
	ret nz			;710d	c0		.
	ret nz			;710e	c0		.
	ret nz			;710f	c0		.
	ret nz			;7110	c0		.
	rlca			;7111	07		.
	rlca			;7112	07		.
	ret nz			;7113	c0		.
	dec hl			;7114	2b		+
	ret nz			;7115	c0		.
	ret nz			;7116	c0		.
	ret nz			;7117	c0		.
	ret nz			;7118	c0		.
	ret nz			;7119	c0		.
	ret nz			;711a	c0		.
	ret nz			;711b	c0		.
	ret nz			;711c	c0		.
	ret nz			;711d	c0		.
	ret nz			;711e	c0		.
	ret nz			;711f	c0		.
	ret nz			;7120	c0		.
	ret nz			;7121	c0		.
	ret nz			;7122	c0		.
	dec hl			;7123	2b		+
	ret nz			;7124	c0		.
	ret nz			;7125	c0		.
	ret nz			;7126	c0		.
	ret nz			;7127	c0		.
	ret nz			;7128	c0		.
	ret nz			;7129	c0		.
l712ah:
	ret nz			;712a	c0		.
	ret nz			;712b	c0		.
	ret nz			;712c	c0		.
	ret nz			;712d	c0		.
	ret nz			;712e	c0		.
	ret nz			;712f	c0		.
	ret nz			;7130	c0		.
	ret nz			;7131	c0		.
	dec hl			;7132	2b		+
	ret nz			;7133	c0		.
	ret nz			;7134	c0		.
	ret nz			;7135	c0		.
	ret nz			;7136	c0		.
	ret nz			;7137	c0		.
	ld de,01111h		;7138	11 11 11	. . .
	ret nz			;713b	c0		.
	ret nz			;713c	c0		.
	ret nz			;713d	c0		.
	ret nz			;713e	c0		.
	ret nz			;713f	c0		.
	ret nz			;7140	c0		.
	dec hl			;7141	2b		+
	ret nz			;7142	c0		.
	ret nz			;7143	c0		.
	ret nz			;7144	c0		.
	ld de,01111h		;7145	11 11 11	. . .
	dec d			;7148	15		.
	ld de,01111h		;7149	11 11 11	. . .
	ret nz			;714c	c0		.
	ret nz			;714d	c0		.
	ret nz			;714e	c0		.
	ret nz			;714f	c0		.
	dec hl			;7150	2b		+
	ret nz			;7151	c0		.
	ret nz			;7152	c0		.
	ld de,01511h		;7153	11 11 15	. . .
	dec d			;7156	15		.
	add hl,bc		;7157	09		.
	dec d			;7158	15		.
	dec d			;7159	15		.
	ld de,lc011h		;715a	11 11 c0	. . .
	ret nz			;715d	c0		.
	ret nz			;715e	c0		.
	dec hl			;715f	2b		+
	ret nz			;7160	c0		.
	ld de,01511h		;7161	11 11 15	. . .
	dec d			;7164	15		.
	add hl,bc		;7165	09		.
	add hl,bc		;7166	09		.
	add hl,bc		;7167	09		.
	dec d			;7168	15		.
	dec d			;7169	15		.
	ld de,lc011h		;716a	11 11 c0	. . .
	ret nz			;716d	c0		.
	dec hl			;716e	2b		+
	ret nz			;716f	c0		.
	ret nz			;7170	c0		.
	ld de,01511h		;7171	11 11 15	. . .
	dec d			;7174	15		.
	add hl,bc		;7175	09		.
	dec d			;7176	15		.
	dec d			;7177	15		.
	ld de,lc011h		;7178	11 11 c0	. . .
	ret nz			;717b	c0		.
	ret nz			;717c	c0		.
	dec hl			;717d	2b		+
	ret nz			;717e	c0		.
	ret nz			;717f	c0		.
	ret nz			;7180	c0		.
	ld de,01111h		;7181	11 11 11	. . .
	dec d			;7184	15		.
	ld de,01111h		;7185	11 11 11	. . .
	ret nz			;7188	c0		.
	ret nz			;7189	c0		.
	ret nz			;718a	c0		.
	ret nz			;718b	c0		.
	dec hl			;718c	2b		+
	ret nz			;718d	c0		.
	ret nz			;718e	c0		.
	ret nz			;718f	c0		.
	ret nz			;7190	c0		.
	ret nz			;7191	c0		.
	ld de,01111h		;7192	11 11 11	. . .
	ret nz			;7195	c0		.
	ret nz			;7196	c0		.
	ret nz			;7197	c0		.
	ret nz			;7198	c0		.
	ret nz			;7199	c0		.
	ret nz			;719a	c0		.
	dec hl			;719b	2b		+
	ret nz			;719c	c0		.
	ret nz			;719d	c0		.
	ret nz			;719e	c0		.
	ret nz			;719f	c0		.
	ret nz			;71a0	c0		.
	ret nz			;71a1	c0		.
	ret nz			;71a2	c0		.
	ret nz			;71a3	c0		.
	ret nz			;71a4	c0		.
	ret nz			;71a5	c0		.
	ret nz			;71a6	c0		.
	ret nz			;71a7	c0		.
	ret nz			;71a8	c0		.
	ret nz			;71a9	c0		.
	dec hl			;71aa	2b		+
	ret nz			;71ab	c0		.
	ret nz			;71ac	c0		.
	ret nz			;71ad	c0		.
	ret nz			;71ae	c0		.
	ret nz			;71af	c0		.
	ret nz			;71b0	c0		.
	ret nz			;71b1	c0		.
	ret nz			;71b2	c0		.
	ret nz			;71b3	c0		.
	ret nz			;71b4	c0		.
	ret nz			;71b5	c0		.
	ret nz			;71b6	c0		.
	ret nz			;71b7	c0		.
	ret nz			;71b8	c0		.
	dec hl			;71b9	2b		+
	dec hl			;71ba	2b		+
	dec hl			;71bb	2b		+
	dec hl			;71bc	2b		+
	dec hl			;71bd	2b		+
	dec hl			;71be	2b		+
	dec hl			;71bf	2b		+
	dec hl			;71c0	2b		+
	dec hl			;71c1	2b		+
	dec hl			;71c2	2b		+
	dec hl			;71c3	2b		+
	dec hl			;71c4	2b		+
	dec hl			;71c5	2b		+
	dec hl			;71c6	2b		+
	ret nz			;71c7	c0		.
	dec hl			;71c8	2b		+
	ret nz			;71c9	c0		.
	dec hl			;71ca	2b		+
	ret nz			;71cb	c0		.
	ret nz			;71cc	c0		.
	ret nz			;71cd	c0		.
	ret nz			;71ce	c0		.
	ret nz			;71cf	c0		.
	ret nz			;71d0	c0		.
	ret nz			;71d1	c0		.
	dec hl			;71d2	2b		+
	ret nz			;71d3	c0		.
	dec hl			;71d4	2b		+
	ret nz			;71d5	c0		.
	ret nz			;71d6	c0		.
	dec hl			;71d7	2b		+
	dec d			;71d8	15		.
	dec hl			;71d9	2b		+
	ret nz			;71da	c0		.
	ret nz			;71db	c0		.
	ret nz			;71dc	c0		.
	ret nz			;71dd	c0		.
	ret nz			;71de	c0		.
	ret nz			;71df	c0		.
	ret nz			;71e0	c0		.
	dec hl			;71e1	2b		+
	dec d			;71e2	15		.
	dec hl			;71e3	2b		+
	ret nz			;71e4	c0		.
	ret nz			;71e5	c0		.
	dec hl			;71e6	2b		+
	inc d			;71e7	14		.
	dec hl			;71e8	2b		+
	ret nz			;71e9	c0		.
	ret nz			;71ea	c0		.
	ret nz			;71eb	c0		.
	ret nz			;71ec	c0		.
	ret nz			;71ed	c0		.
	ret nz			;71ee	c0		.
	ret nz			;71ef	c0		.
	dec hl			;71f0	2b		+
	inc d			;71f1	14		.
	dec hl			;71f2	2b		+
	ret nz			;71f3	c0		.
	ret nz			;71f4	c0		.
	dec hl			;71f5	2b		+
	dec hl			;71f6	2b		+
	dec hl			;71f7	2b		+
	ret nz			;71f8	c0		.
	ret nz			;71f9	c0		.
	ret nz			;71fa	c0		.
	ret nz			;71fb	c0		.
	ret nz			;71fc	c0		.
	ret nz			;71fd	c0		.
	ret nz			;71fe	c0		.
	dec hl			;71ff	2b		+
	dec hl			;7200	2b		+
	dec hl			;7201	2b		+
	ret nz			;7202	c0		.
	ret nz			;7203	c0		.
	ret nz			;7204	c0		.
	ret nz			;7205	c0		.
	ret nz			;7206	c0		.
	ret nz			;7207	c0		.
	ret nz			;7208	c0		.
	ex af,af'		;7209	08		.
	ex af,af'		;720a	08		.
	ex af,af'		;720b	08		.
	ret nz			;720c	c0		.
	ret nz			;720d	c0		.
	ret nz			;720e	c0		.
	ret nz			;720f	c0		.
	ret nz			;7210	c0		.
	ret nz			;7211	c0		.
	ret nz			;7212	c0		.
	ret nz			;7213	c0		.
	ret nz			;7214	c0		.
	ret nz			;7215	c0		.
	ex af,af'		;7216	08		.
	ex af,af'		;7217	08		.
	inc d			;7218	14		.
	inc d			;7219	14		.
	inc d			;721a	14		.
	ex af,af'		;721b	08		.
	ex af,af'		;721c	08		.
	ret nz			;721d	c0		.
	ret nz			;721e	c0		.
	ret nz			;721f	c0		.
	ret nz			;7220	c0		.
	ret nz			;7221	c0		.
	ret nz			;7222	c0		.
	ex af,af'		;7223	08		.
	ex af,af'		;7224	08		.
	inc d			;7225	14		.
	inc d			;7226	14		.
	dec d			;7227	15		.
	dec d			;7228	15		.
	dec d			;7229	15		.
	inc d			;722a	14		.
	inc d			;722b	14		.
	ex af,af'		;722c	08		.
	ex af,af'		;722d	08		.
	ret nz			;722e	c0		.
	ret nz			;722f	c0		.
	ret nz			;7230	c0		.
	ret nz			;7231	c0		.
	ex af,af'		;7232	08		.
	ex af,af'		;7233	08		.
	inc d			;7234	14		.
	inc d			;7235	14		.
	dec d			;7236	15		.
	dec d			;7237	15		.
	dec d			;7238	15		.
	inc d			;7239	14		.
	inc d			;723a	14		.
	ex af,af'		;723b	08		.
	ex af,af'		;723c	08		.
	ret nz			;723d	c0		.
	ret nz			;723e	c0		.
	ret nz			;723f	c0		.
	ret nz			;7240	c0		.
	ret nz			;7241	c0		.
	ret nz			;7242	c0		.
	ex af,af'		;7243	08		.
	ex af,af'		;7244	08		.
	inc d			;7245	14		.
	inc d			;7246	14		.
	inc d			;7247	14		.
	ex af,af'		;7248	08		.
	ex af,af'		;7249	08		.
	ret nz			;724a	c0		.
	ret nz			;724b	c0		.
	ret nz			;724c	c0		.
	ret nz			;724d	c0		.
	add hl,bc		;724e	09		.
	ret nz			;724f	c0		.
	ret nz			;7250	c0		.
	ret nz			;7251	c0		.
	ret nz			;7252	c0		.
	ret nz			;7253	c0		.
	ex af,af'		;7254	08		.
	ex af,af'		;7255	08		.
	ex af,af'		;7256	08		.
	ret nz			;7257	c0		.
	ret nz			;7258	c0		.
	ret nz			;7259	c0		.
	ret nz			;725a	c0		.
	ret nz			;725b	c0		.
	add hl,bc		;725c	09		.
	ret nz			;725d	c0		.
	add hl,bc		;725e	09		.
	ret nz			;725f	c0		.
	ret nz			;7260	c0		.
	ret nz			;7261	c0		.
	ret nz			;7262	c0		.
	ret nz			;7263	c0		.
	ret nz			;7264	c0		.
	ret nz			;7265	c0		.
	ret nz			;7266	c0		.
	ret nz			;7267	c0		.
	ret nz			;7268	c0		.
	ret nz			;7269	c0		.
	add hl,bc		;726a	09		.
	ret nz			;726b	c0		.
	ret nz			;726c	c0		.
	ret nz			;726d	c0		.
	add hl,bc		;726e	09		.
	ret nz			;726f	c0		.
	ret nz			;7270	c0		.
	ret nz			;7271	c0		.
	ret nz			;7272	c0		.
	ret nz			;7273	c0		.
	ret nz			;7274	c0		.
	ret nz			;7275	c0		.
	ret nz			;7276	c0		.
	ret nz			;7277	c0		.
	add hl,bc		;7278	09		.
	ret nz			;7279	c0		.
	ret nz			;727a	c0		.
	ret nz			;727b	c0		.
	ret nz			;727c	c0		.
	inc l			;727d	2c		,
	ret nz			;727e	c0		.
	ret nz			;727f	c0		.
	ret nz			;7280	c0		.
	ret nz			;7281	c0		.
	ret nz			;7282	c0		.
	ret nz			;7283	c0		.
	ret nz			;7284	c0		.
	ret nz			;7285	c0		.
	ret nz			;7286	c0		.
	inc l			;7287	2c		,
	ret nz			;7288	c0		.
	ret nz			;7289	c0		.
	ret nz			;728a	c0		.
	ret nz			;728b	c0		.
	ret nz			;728c	c0		.
	ret nz			;728d	c0		.
	ret nz			;728e	c0		.
	ret nz			;728f	c0		.
	ret nz			;7290	c0		.
	ret nz			;7291	c0		.
	ret nz			;7292	c0		.
	ret nz			;7293	c0		.
	ret nz			;7294	c0		.
	ret nz			;7295	c0		.
	ret nz			;7296	c0		.
	ret nz			;7297	c0		.
	ret nz			;7298	c0		.
	inc l			;7299	2c		,
	ret nz			;729a	c0		.
	ld b,0c0h		;729b	06 c0		. .
	inc l			;729d	2c		,
	ret nz			;729e	c0		.
	ret nz			;729f	c0		.
	ret nz			;72a0	c0		.
	ret nz			;72a1	c0		.
	ret nz			;72a2	c0		.
	inc l			;72a3	2c		,
	ret nz			;72a4	c0		.
	ld b,0c0h		;72a5	06 c0		. .
	inc l			;72a7	2c		,
	ret nz			;72a8	c0		.
	ret nz			;72a9	c0		.
	ret nz			;72aa	c0		.
	ret nz			;72ab	c0		.
	ret nz			;72ac	c0		.
	ret nz			;72ad	c0		.
	ret nz			;72ae	c0		.
	ret nz			;72af	c0		.
	ret nz			;72b0	c0		.
	ret nz			;72b1	c0		.
	ret nz			;72b2	c0		.
	ret nz			;72b3	c0		.
	ret nz			;72b4	c0		.
	ret nz			;72b5	c0		.
	ret nz			;72b6	c0		.
	ret nz			;72b7	c0		.
	ret nz			;72b8	c0		.
	inc l			;72b9	2c		,
	ret nz			;72ba	c0		.
	ret nz			;72bb	c0		.
	ret nz			;72bc	c0		.
	ret nz			;72bd	c0		.
	ret nz			;72be	c0		.
	ret nz			;72bf	c0		.
	ret nz			;72c0	c0		.
	ret nz			;72c1	c0		.
	ret nz			;72c2	c0		.
	inc l			;72c3	2c		,
	ret nz			;72c4	c0		.
	ret nz			;72c5	c0		.
	ret nz			;72c6	c0		.
	ret nz			;72c7	c0		.
	ret nz			;72c8	c0		.
	ret nz			;72c9	c0		.
	ret nz			;72ca	c0		.
	ret nz			;72cb	c0		.
	ret nz			;72cc	c0		.
	dec l			;72cd	2d		-
	ret nz			;72ce	c0		.
	ret nz			;72cf	c0		.
	ret nz			;72d0	c0		.
	ret nz			;72d1	c0		.
	ret nz			;72d2	c0		.
	ret nz			;72d3	c0		.
	ret nz			;72d4	c0		.
	ret nz			;72d5	c0		.
	ret nz			;72d6	c0		.
	ret nz			;72d7	c0		.
	ret nz			;72d8	c0		.
	ret nz			;72d9	c0		.
	ret nz			;72da	c0		.
	ret nz			;72db	c0		.
	ex af,af'		;72dc	08		.
	ret nz			;72dd	c0		.
	ret nz			;72de	c0		.
	ret nz			;72df	c0		.
	ret nz			;72e0	c0		.
	ret nz			;72e1	c0		.
	ret nz			;72e2	c0		.
	ret nz			;72e3	c0		.
	ret nz			;72e4	c0		.
	dec l			;72e5	2d		-
	ret nz			;72e6	c0		.
	ret nz			;72e7	c0		.
	ret nz			;72e8	c0		.
	dec l			;72e9	2d		-
	ex af,af'		;72ea	08		.
	ld b,008h		;72eb	06 08		. .
	dec l			;72ed	2d		-
	ret nz			;72ee	c0		.
	ret nz			;72ef	c0		.
	ret nz			;72f0	c0		.
	dec l			;72f1	2d		-
	ret nz			;72f2	c0		.
	ret nz			;72f3	c0		.
	ret nz			;72f4	c0		.
	ret nz			;72f5	c0		.
	ret nz			;72f6	c0		.
	ret nz			;72f7	c0		.
	ret nz			;72f8	c0		.
	ret nz			;72f9	c0		.
	ex af,af'		;72fa	08		.
	ret nz			;72fb	c0		.
	ret nz			;72fc	c0		.
	ret nz			;72fd	c0		.
	ret nz			;72fe	c0		.
	ret nz			;72ff	c0		.
	ret nz			;7300	c0		.
	ret nz			;7301	c0		.
	ret nz			;7302	c0		.
	ret nz			;7303	c0		.
	ret nz			;7304	c0		.
	dec l			;7305	2d		-
	ret nz			;7306	c0		.
	ret nz			;7307	c0		.
	ret nz			;7308	c0		.
	dec l			;7309	2d		-
	ret nz			;730a	c0		.
	ret nz			;730b	c0		.
	ret nz			;730c	c0		.
	dec l			;730d	2d		-
	ret nz			;730e	c0		.
	ret nz			;730f	c0		.
	ret nz			;7310	c0		.
	ret nz			;7311	c0		.
	ret nz			;7312	c0		.
	ret nz			;7313	c0		.
	ret nz			;7314	c0		.
	ret nz			;7315	c0		.
	ret nz			;7316	c0		.
	ret nz			;7317	c0		.
	ret nz			;7318	c0		.
	ret nz			;7319	c0		.
	ret nz			;731a	c0		.
	ret nz			;731b	c0		.
	ret nz			;731c	c0		.
	ret nz			;731d	c0		.
	ret nz			;731e	c0		.
	ret nz			;731f	c0		.
	ret nz			;7320	c0		.
	ret nz			;7321	c0		.
	ret nz			;7322	c0		.
	ret nz			;7323	c0		.
	ret nz			;7324	c0		.
	ret nz			;7325	c0		.
	ret nz			;7326	c0		.
	ret nz			;7327	c0		.
	ret nz			;7328	c0		.
	ret nz			;7329	c0		.
	ret nz			;732a	c0		.
	ret nz			;732b	c0		.
	ret nz			;732c	c0		.
	ret nz			;732d	c0		.
	ret nz			;732e	c0		.
	ret nz			;732f	c0		.
	ret nz			;7330	c0		.
	ret nz			;7331	c0		.
	dec d			;7332	15		.
	ret nz			;7333	c0		.
	ret nz			;7334	c0		.
	ret nz			;7335	c0		.
	ret nz			;7336	c0		.
	ret nz			;7337	c0		.
	ret nz			;7338	c0		.
	ret nz			;7339	c0		.
l733ah:
	dec d			;733a	15		.
	ret nz			;733b	c0		.
	ret nz			;733c	c0		.
	ret nz			;733d	c0		.
	ret nz			;733e	c0		.
	ret nz			;733f	c0		.
	dec d			;7340	15		.
	inc d			;7341	14		.
	dec d			;7342	15		.
	ret nz			;7343	c0		.
	ret nz			;7344	c0		.
	ret nz			;7345	c0		.
	ret nz			;7346	c0		.
	ret nz			;7347	c0		.
	dec d			;7348	15		.
	inc d			;7349	14		.
	dec d			;734a	15		.
	ret nz			;734b	c0		.
	ret nz			;734c	c0		.
	ret nz			;734d	c0		.
	dec d			;734e	15		.
	inc d			;734f	14		.
	add hl,bc		;7350	09		.
	inc d			;7351	14		.
	dec d			;7352	15		.
	ret nz			;7353	c0		.
	ret nz			;7354	c0		.
	ret nz			;7355	c0		.
	dec d			;7356	15		.
	inc d			;7357	14		.
	add hl,bc		;7358	09		.
	inc d			;7359	14		.
	dec d			;735a	15		.
	ret nz			;735b	c0		.
	dec d			;735c	15		.
	inc d			;735d	14		.
	add hl,bc		;735e	09		.
	ex af,af'		;735f	08		.
	add hl,bc		;7360	09		.
	inc d			;7361	14		.
	dec d			;7362	15		.
	ret nz			;7363	c0		.
	dec d			;7364	15		.
	inc d			;7365	14		.
	add hl,bc		;7366	09		.
	ex af,af'		;7367	08		.
	add hl,bc		;7368	09		.
	inc d			;7369	14		.
	dec d			;736a	15		.
	ret nz			;736b	c0		.
	dec d			;736c	15		.
	inc d			;736d	14		.
	add hl,bc		;736e	09		.
	inc d			;736f	14		.
	dec d			;7370	15		.
	ret nz			;7371	c0		.
	ret nz			;7372	c0		.
	ret nz			;7373	c0		.
	dec d			;7374	15		.
	inc d			;7375	14		.
	add hl,bc		;7376	09		.
	inc d			;7377	14		.
	dec d			;7378	15		.
	ret nz			;7379	c0		.
	ret nz			;737a	c0		.
	ret nz			;737b	c0		.
	dec d			;737c	15		.
	inc d			;737d	14		.
	dec d			;737e	15		.
	ret nz			;737f	c0		.
	ret nz			;7380	c0		.
	ret nz			;7381	c0		.
	ret nz			;7382	c0		.
	ret nz			;7383	c0		.
	dec d			;7384	15		.
	inc d			;7385	14		.
	dec d			;7386	15		.
	ret nz			;7387	c0		.
	ret nz			;7388	c0		.
	ret nz			;7389	c0		.
	ret nz			;738a	c0		.
	ret nz			;738b	c0		.
	dec d			;738c	15		.
	ret nz			;738d	c0		.
	ret nz			;738e	c0		.
	ret nz			;738f	c0		.
	ret nz			;7390	c0		.
	ret nz			;7391	c0		.
	ret nz			;7392	c0		.
	ret nz			;7393	c0		.
	dec d			;7394	15		.
	ret nz			;7395	c0		.
	ret nz			;7396	c0		.
	ret nz			;7397	c0		.
	ret nz			;7398	c0		.
	ret nz			;7399	c0		.
	ret nz			;739a	c0		.
	ret nz			;739b	c0		.
	ret nz			;739c	c0		.
	ret nz			;739d	c0		.
	ret nz			;739e	c0		.
	ret nz			;739f	c0		.
	ret nz			;73a0	c0		.
	ret nz			;73a1	c0		.
	ret nz			;73a2	c0		.
	ret nz			;73a3	c0		.
	ret nz			;73a4	c0		.
	ret nz			;73a5	c0		.
	ret nz			;73a6	c0		.
	dec hl			;73a7	2b		+
	dec hl			;73a8	2b		+
	ret nz			;73a9	c0		.
	dec hl			;73aa	2b		+
	dec hl			;73ab	2b		+
	dec hl			;73ac	2b		+
	dec hl			;73ad	2b		+
	dec hl			;73ae	2b		+
	dec hl			;73af	2b		+
	dec hl			;73b0	2b		+
	dec hl			;73b1	2b		+
	dec hl			;73b2	2b		+
	ret nz			;73b3	c0		.
	dec hl			;73b4	2b		+
	dec hl			;73b5	2b		+
	ret nz			;73b6	c0		.
	ret nz			;73b7	c0		.
	ret nz			;73b8	c0		.
	dec hl			;73b9	2b		+
	ret nz			;73ba	c0		.
	ret nz			;73bb	c0		.
	ret nz			;73bc	c0		.
	ret nz			;73bd	c0		.
	ret nz			;73be	c0		.
	ret nz			;73bf	c0		.
	ret nz			;73c0	c0		.
	dec hl			;73c1	2b		+
	ret nz			;73c2	c0		.
	ret nz			;73c3	c0		.
	ret nz			;73c4	c0		.
	ret nz			;73c5	c0		.
	ret nz			;73c6	c0		.
	ret nz			;73c7	c0		.
	dec hl			;73c8	2b		+
	ret nz			;73c9	c0		.
	ret nz			;73ca	c0		.
	ret nz			;73cb	c0		.
	ret nz			;73cc	c0		.
	ret nz			;73cd	c0		.
	ret nz			;73ce	c0		.
	ret nz			;73cf	c0		.
	dec hl			;73d0	2b		+
	ret nz			;73d1	c0		.
	ret nz			;73d2	c0		.
	ret nz			;73d3	c0		.
	ret nz			;73d4	c0		.
	dec hl			;73d5	2b		+
	dec hl			;73d6	2b		+
	dec hl			;73d7	2b		+
	ret nz			;73d8	c0		.
	ret nz			;73d9	c0		.
	ret nz			;73da	c0		.
	ret nz			;73db	c0		.
	ret nz			;73dc	c0		.
	ret nz			;73dd	c0		.
	ret nz			;73de	c0		.
	dec hl			;73df	2b		+
	dec hl			;73e0	2b		+
	dec hl			;73e1	2b		+
	ret nz			;73e2	c0		.
	ret nz			;73e3	c0		.
	rlca			;73e4	07		.
	rlca			;73e5	07		.
	rlca			;73e6	07		.
	rlca			;73e7	07		.
	rlca			;73e8	07		.
	rlca			;73e9	07		.
	rlca			;73ea	07		.
	rlca			;73eb	07		.
	rlca			;73ec	07		.
	rlca			;73ed	07		.
	rlca			;73ee	07		.
	rlca			;73ef	07		.
	rlca			;73f0	07		.
	ret nz			;73f1	c0		.
	ret nz			;73f2	c0		.
	rlca			;73f3	07		.
	ret nz			;73f4	c0		.
	ret nz			;73f5	c0		.
	ret nz			;73f6	c0		.
	ret nz			;73f7	c0		.
	ret nz			;73f8	c0		.
	ret nz			;73f9	c0		.
	ret nz			;73fa	c0		.
	ret nz			;73fb	c0		.
	ret nz			;73fc	c0		.
	ret nz			;73fd	c0		.
	ret nz			;73fe	c0		.
	rlca			;73ff	07		.
l7400h:
	ret nz			;7400	c0		.
	ret nz			;7401	c0		.
	rlca			;7402	07		.
	ret nz			;7403	c0		.
	ret nz			;7404	c0		.
	ret nz			;7405	c0		.
	ret nz			;7406	c0		.
	ret nz			;7407	c0		.
	ret nz			;7408	c0		.
	ret nz			;7409	c0		.
	ret nz			;740a	c0		.
	ret nz			;740b	c0		.
	ret nz			;740c	c0		.
	ret nz			;740d	c0		.
	rlca			;740e	07		.
	ret nz			;740f	c0		.
	ret nz			;7410	c0		.
	rlca			;7411	07		.
	ret nz			;7412	c0		.
	add hl,bc		;7413	09		.
	add hl,bc		;7414	09		.
	add hl,bc		;7415	09		.
	add hl,bc		;7416	09		.
	add hl,bc		;7417	09		.
	add hl,bc		;7418	09		.
	add hl,bc		;7419	09		.
	add hl,bc		;741a	09		.
	add hl,bc		;741b	09		.
	ret nz			;741c	c0		.
	rlca			;741d	07		.
	ret nz			;741e	c0		.
	ret nz			;741f	c0		.
	rlca			;7420	07		.
	ret nz			;7421	c0		.
	add hl,bc		;7422	09		.
	ret nz			;7423	c0		.
	ret nz			;7424	c0		.
	ret nz			;7425	c0		.
	ret nz			;7426	c0		.
	ret nz			;7427	c0		.
	ret nz			;7428	c0		.
	ret nz			;7429	c0		.
	add hl,bc		;742a	09		.
	ret nz			;742b	c0		.
	rlca			;742c	07		.
	ret nz			;742d	c0		.
	ret nz			;742e	c0		.
	rlca			;742f	07		.
	ret nz			;7430	c0		.
	add hl,bc		;7431	09		.
	ret nz			;7432	c0		.
	ex af,af'		;7433	08		.
	ex af,af'		;7434	08		.
	ex af,af'		;7435	08		.
	ex af,af'		;7436	08		.
	ex af,af'		;7437	08		.
	ret nz			;7438	c0		.
	add hl,bc		;7439	09		.
	ret nz			;743a	c0		.
	rlca			;743b	07		.
	ret nz			;743c	c0		.
	ret nz			;743d	c0		.
	rlca			;743e	07		.
	ret nz			;743f	c0		.
	add hl,bc		;7440	09		.
	ret nz			;7441	c0		.
	ex af,af'		;7442	08		.
	ex af,af'		;7443	08		.
	ex af,af'		;7444	08		.
	ex af,af'		;7445	08		.
	ex af,af'		;7446	08		.
	ret nz			;7447	c0		.
	add hl,bc		;7448	09		.
	ret nz			;7449	c0		.
	rlca			;744a	07		.
	ret nz			;744b	c0		.
	ret nz			;744c	c0		.
	rlca			;744d	07		.
	ret nz			;744e	c0		.
	add hl,bc		;744f	09		.
	ret nz			;7450	c0		.
	ret nz			;7451	c0		.
	ret nz			;7452	c0		.
	ret nz			;7453	c0		.
	ret nz			;7454	c0		.
	ret nz			;7455	c0		.
	ret nz			;7456	c0		.
	add hl,bc		;7457	09		.
	ret nz			;7458	c0		.
	rlca			;7459	07		.
	ret nz			;745a	c0		.
	ret nz			;745b	c0		.
	rlca			;745c	07		.
	ret nz			;745d	c0		.
	add hl,bc		;745e	09		.
	add hl,bc		;745f	09		.
	add hl,bc		;7460	09		.
	add hl,bc		;7461	09		.
	add hl,bc		;7462	09		.
	add hl,bc		;7463	09		.
	add hl,bc		;7464	09		.
	add hl,bc		;7465	09		.
	add hl,bc		;7466	09		.
	ret nz			;7467	c0		.
	rlca			;7468	07		.
	ret nz			;7469	c0		.
	ret nz			;746a	c0		.
	rlca			;746b	07		.
	ret nz			;746c	c0		.
	ret nz			;746d	c0		.
	ret nz			;746e	c0		.
	ret nz			;746f	c0		.
	ret nz			;7470	c0		.
	ret nz			;7471	c0		.
	ret nz			;7472	c0		.
	ret nz			;7473	c0		.
	ret nz			;7474	c0		.
	ret nz			;7475	c0		.
	ret nz			;7476	c0		.
	rlca			;7477	07		.
	ret nz			;7478	c0		.
	ret nz			;7479	c0		.
	rlca			;747a	07		.
	ret nz			;747b	c0		.
	ret nz			;747c	c0		.
	ret nz			;747d	c0		.
	ret nz			;747e	c0		.
	ret nz			;747f	c0		.
	ret nz			;7480	c0		.
	ret nz			;7481	c0		.
	ret nz			;7482	c0		.
	ret nz			;7483	c0		.
	ret nz			;7484	c0		.
	ret nz			;7485	c0		.
	rlca			;7486	07		.
	ret nz			;7487	c0		.
	ret nz			;7488	c0		.
	rlca			;7489	07		.
	rlca			;748a	07		.
	rlca			;748b	07		.
	rlca			;748c	07		.
	rlca			;748d	07		.
	rlca			;748e	07		.
	rlca			;748f	07		.
	rlca			;7490	07		.
	rlca			;7491	07		.
	rlca			;7492	07		.
	rlca			;7493	07		.
	rlca			;7494	07		.
	rlca			;7495	07		.
	ret nz			;7496	c0		.
	ret nz			;7497	c0		.
	ret nz			;7498	c0		.
	dec hl			;7499	2b		+
	add hl,bc		;749a	09		.
	rlca			;749b	07		.
	inc d			;749c	14		.
	ret nz			;749d	c0		.
	ret nz			;749e	c0		.
	ret nz			;749f	c0		.
	inc d			;74a0	14		.
	rlca			;74a1	07		.
	add hl,bc		;74a2	09		.
	dec hl			;74a3	2b		+
	ret nz			;74a4	c0		.
	ret nz			;74a5	c0		.
	ret nz			;74a6	c0		.
	ret nz			;74a7	c0		.
	dec hl			;74a8	2b		+
	dec d			;74a9	15		.
	add hl,bc		;74aa	09		.
	rlca			;74ab	07		.
	inc d			;74ac	14		.
	ret nz			;74ad	c0		.
	inc d			;74ae	14		.
	rlca			;74af	07		.
	add hl,bc		;74b0	09		.
	dec d			;74b1	15		.
	dec hl			;74b2	2b		+
	ret nz			;74b3	c0		.
	ret nz			;74b4	c0		.
	ret nz			;74b5	c0		.
	ret nz			;74b6	c0		.
	ret nz			;74b7	c0		.
	dec hl			;74b8	2b		+
	dec d			;74b9	15		.
	add hl,bc		;74ba	09		.
	rlca			;74bb	07		.
	inc d			;74bc	14		.
	rlca			;74bd	07		.
	add hl,bc		;74be	09		.
	dec d			;74bf	15		.
	dec hl			;74c0	2b		+
	ret nz			;74c1	c0		.
	ret nz			;74c2	c0		.
	ret nz			;74c3	c0		.
	ret nz			;74c4	c0		.
	ret nz			;74c5	c0		.
	ret nz			;74c6	c0		.
	ret nz			;74c7	c0		.
	dec hl			;74c8	2b		+
	dec d			;74c9	15		.
	add hl,bc		;74ca	09		.
	rlca			;74cb	07		.
	add hl,bc		;74cc	09		.
	dec d			;74cd	15		.
	dec hl			;74ce	2b		+
	ret nz			;74cf	c0		.
	ret nz			;74d0	c0		.
	ret nz			;74d1	c0		.
	ret nz			;74d2	c0		.
	ret nz			;74d3	c0		.
	ret nz			;74d4	c0		.
	ret nz			;74d5	c0		.
	ret nz			;74d6	c0		.
	ret nz			;74d7	c0		.
	dec hl			;74d8	2b		+
	dec d			;74d9	15		.
	add hl,bc		;74da	09		.
	dec d			;74db	15		.
	dec hl			;74dc	2b		+
	ret nz			;74dd	c0		.
	ret nz			;74de	c0		.
	ret nz			;74df	c0		.
	ret nz			;74e0	c0		.
	ret nz			;74e1	c0		.
	ret nz			;74e2	c0		.
	ret nz			;74e3	c0		.
	ret nz			;74e4	c0		.
	ret nz			;74e5	c0		.
	ret nz			;74e6	c0		.
	dec hl			;74e7	2b		+
	dec d			;74e8	15		.
	add hl,bc		;74e9	09		.
	dec d			;74ea	15		.
	dec hl			;74eb	2b		+
	ret nz			;74ec	c0		.
	ret nz			;74ed	c0		.
	ret nz			;74ee	c0		.
	ret nz			;74ef	c0		.
	ret nz			;74f0	c0		.
	ret nz			;74f1	c0		.
	ret nz			;74f2	c0		.
	ret nz			;74f3	c0		.
	ret nz			;74f4	c0		.
	ret nz			;74f5	c0		.
	dec hl			;74f6	2b		+
	dec d			;74f7	15		.
	add hl,bc		;74f8	09		.
	dec d			;74f9	15		.
	dec hl			;74fa	2b		+
	ret nz			;74fb	c0		.
	ret nz			;74fc	c0		.
	ret nz			;74fd	c0		.
	ret nz			;74fe	c0		.
	ret nz			;74ff	c0		.
	ret nz			;7500	c0		.
	ret nz			;7501	c0		.
	ret nz			;7502	c0		.
	ret nz			;7503	c0		.
	ret nz			;7504	c0		.
	dec hl			;7505	2b		+
	dec d			;7506	15		.
	add hl,bc		;7507	09		.
	dec d			;7508	15		.
	dec hl			;7509	2b		+
	ret nz			;750a	c0		.
	ret nz			;750b	c0		.
	ret nz			;750c	c0		.
	ret nz			;750d	c0		.
	ret nz			;750e	c0		.
	ret nz			;750f	c0		.
	ret nz			;7510	c0		.
	ret nz			;7511	c0		.
	ret nz			;7512	c0		.
	ret nz			;7513	c0		.
	dec hl			;7514	2b		+
	dec d			;7515	15		.
	add hl,bc		;7516	09		.
	dec d			;7517	15		.
	dec hl			;7518	2b		+
	ret nz			;7519	c0		.
	ret nz			;751a	c0		.
	ret nz			;751b	c0		.
	ret nz			;751c	c0		.
	ret nz			;751d	c0		.
	ret nz			;751e	c0		.
	ret nz			;751f	c0		.
	ret nz			;7520	c0		.
	ret nz			;7521	c0		.
	ret nz			;7522	c0		.
	dec hl			;7523	2b		+
	dec d			;7524	15		.
	add hl,bc		;7525	09		.
	dec d			;7526	15		.
	dec hl			;7527	2b		+
	ret nz			;7528	c0		.
	ret nz			;7529	c0		.
	ret nz			;752a	c0		.
	ret nz			;752b	c0		.
	ret nz			;752c	c0		.
	ret nz			;752d	c0		.
	ret nz			;752e	c0		.
	ret nz			;752f	c0		.
	ret nz			;7530	c0		.
	ret nz			;7531	c0		.
	dec hl			;7532	2b		+
	dec d			;7533	15		.
	dec d			;7534	15		.
	dec d			;7535	15		.
	dec hl			;7536	2b		+
	ret nz			;7537	c0		.
	ret nz			;7538	c0		.
	ret nz			;7539	c0		.
	ret nz			;753a	c0		.
	ret nz			;753b	c0		.
	ret nz			;753c	c0		.
	ret nz			;753d	c0		.
	ret nz			;753e	c0		.
	ret nz			;753f	c0		.
	ret nz			;7540	c0		.
	dec hl			;7541	2b		+
	dec hl			;7542	2b		+
	dec hl			;7543	2b		+
	dec hl			;7544	2b		+
	dec hl			;7545	2b		+
	ret nz			;7546	c0		.
	ret nz			;7547	c0		.
	ret nz			;7548	c0		.
	ret nz			;7549	c0		.
	ret nz			;754a	c0		.
	ret nz			;754b	c0		.
	ret nz			;754c	c0		.
	ret nz			;754d	c0		.
	ret nz			;754e	c0		.
	ret nz			;754f	c0		.
	dec d			;7550	15		.
	ret nz			;7551	c0		.
	ret nz			;7552	c0		.
	ret nz			;7553	c0		.
	dec d			;7554	15		.
	ret nz			;7555	c0		.
	ret nz			;7556	c0		.
	ret nz			;7557	c0		.
	ret nz			;7558	c0		.
	ret nz			;7559	c0		.
	ret nz			;755a	c0		.
	ret nz			;755b	c0		.
	ret nz			;755c	c0		.
	ret nz			;755d	c0		.
	ret nz			;755e	c0		.
	ret nz			;755f	c0		.
	dec d			;7560	15		.
	ret nz			;7561	c0		.
	dec d			;7562	15		.
	ret nz			;7563	c0		.
	ret nz			;7564	c0		.
	ret nz			;7565	c0		.
	ret nz			;7566	c0		.
	ret nz			;7567	c0		.
	ret nz			;7568	c0		.
	ret nz			;7569	c0		.
	ret nz			;756a	c0		.
	ret nz			;756b	c0		.
	ret nz			;756c	c0		.
	ret nz			;756d	c0		.
	ret nz			;756e	c0		.
	inc d			;756f	14		.
	inc d			;7570	14		.
	inc d			;7571	14		.
	ret nz			;7572	c0		.
	ret nz			;7573	c0		.
	ret nz			;7574	c0		.
	ret nz			;7575	c0		.
	ret nz			;7576	c0		.
	ret nz			;7577	c0		.
	ret nz			;7578	c0		.
	ret nz			;7579	c0		.
	ret nz			;757a	c0		.
	ld (de),a		;757b	12		.
	ld (de),a		;757c	12		.
	ld (de),a		;757d	12		.
	ld (de),a		;757e	12		.
	ld (de),a		;757f	12		.
	ld (de),a		;7580	12		.
	ld (de),a		;7581	12		.
	ld (de),a		;7582	12		.
	ld (de),a		;7583	12		.
	ret nz			;7584	c0		.
	ret nz			;7585	c0		.
	ret nz			;7586	c0		.
	ret nz			;7587	c0		.
	ret nz			;7588	c0		.
	ld (de),a		;7589	12		.
	ld (de),a		;758a	12		.
	ld (de),a		;758b	12		.
	ld (de),a		;758c	12		.
	ld (de),a		;758d	12		.
	ld (de),a		;758e	12		.
	ld (de),a		;758f	12		.
	ld (de),a		;7590	12		.
	ld (de),a		;7591	12		.
	ld (de),a		;7592	12		.
	ld (de),a		;7593	12		.
	ret nz			;7594	c0		.
	ret nz			;7595	c0		.
	ret nz			;7596	c0		.
	ld b,006h		;7597	06 06		. .
	inc de			;7599	13		.
	ld b,006h		;759a	06 06		. .
	ld b,013h		;759c	06 13		. .
	ld b,006h		;759e	06 06		. .
	ld b,013h		;75a0	06 13		. .
	ld b,006h		;75a2	06 06		. .
	ret nz			;75a4	c0		.
	ret nz			;75a5	c0		.
	ld b,006h		;75a6	06 06		. .
	inc de			;75a8	13		.
	ld b,006h		;75a9	06 06		. .
	ld b,013h		;75ab	06 13		. .
	ld b,006h		;75ad	06 06		. .
	ld b,013h		;75af	06 13		. .
	ld b,006h		;75b1	06 06		. .
	ret nz			;75b3	c0		.
	ret nz			;75b4	c0		.
	ret nz			;75b5	c0		.
	rlca			;75b6	07		.
	rlca			;75b7	07		.
	rlca			;75b8	07		.
	rlca			;75b9	07		.
	rlca			;75ba	07		.
	rlca			;75bb	07		.
	rlca			;75bc	07		.
	rlca			;75bd	07		.
	rlca			;75be	07		.
	rlca			;75bf	07		.
	rlca			;75c0	07		.
	ret nz			;75c1	c0		.
	ret nz			;75c2	c0		.
	ret nz			;75c3	c0		.
	ret nz			;75c4	c0		.
	ret nz			;75c5	c0		.
	rlca			;75c6	07		.
	rlca			;75c7	07		.
	rlca			;75c8	07		.
	rlca			;75c9	07		.
	rlca			;75ca	07		.
	rlca			;75cb	07		.
	rlca			;75cc	07		.
	rlca			;75cd	07		.
	rlca			;75ce	07		.
	ret nz			;75cf	c0		.
	ret nz			;75d0	c0		.
	ret nz			;75d1	c0		.
	ret nz			;75d2	c0		.
	ret nz			;75d3	c0		.
	ret nz			;75d4	c0		.
	ret nz			;75d5	c0		.
	rlca			;75d6	07		.
	rlca			;75d7	07		.
	rlca			;75d8	07		.
	rlca			;75d9	07		.
	rlca			;75da	07		.
	rlca			;75db	07		.
	rlca			;75dc	07		.
	ret nz			;75dd	c0		.
	ret nz			;75de	c0		.
	ret nz			;75df	c0		.
	ret nz			;75e0	c0		.
	ret nz			;75e1	c0		.
	ret nz			;75e2	c0		.
	ret nz			;75e3	c0		.
	ret nz			;75e4	c0		.
	ret nz			;75e5	c0		.
	rlca			;75e6	07		.
	rlca			;75e7	07		.
	rlca			;75e8	07		.
	rlca			;75e9	07		.
	rlca			;75ea	07		.
	ret nz			;75eb	c0		.
	ret nz			;75ec	c0		.
	ret nz			;75ed	c0		.
	ret nz			;75ee	c0		.
	ret nz			;75ef	c0		.
	ret nz			;75f0	c0		.
	ret nz			;75f1	c0		.
	ret nz			;75f2	c0		.
	ret nz			;75f3	c0		.
	ret nz			;75f4	c0		.
	ret nz			;75f5	c0		.
	ret nz			;75f6	c0		.
	ret nz			;75f7	c0		.
	ret nz			;75f8	c0		.
	ret nz			;75f9	c0		.
	ret nz			;75fa	c0		.
	ret nz			;75fb	c0		.
	ret nz			;75fc	c0		.
	ret nz			;75fd	c0		.
	ret nz			;75fe	c0		.
	ret nz			;75ff	c0		.
	ret nz			;7600	c0		.
	ret nz			;7601	c0		.
	ret nz			;7602	c0		.
	ret nz			;7603	c0		.
	ret nz			;7604	c0		.
	ret nz			;7605	c0		.
	ret nz			;7606	c0		.
	ret nz			;7607	c0		.
	ret nz			;7608	c0		.
	ret nz			;7609	c0		.
	ret nz			;760a	c0		.
	ret nz			;760b	c0		.
	ret nz			;760c	c0		.
	ret nz			;760d	c0		.
	ret nz			;760e	c0		.
	ret nz			;760f	c0		.
	dec d			;7610	15		.
	dec d			;7611	15		.
	dec d			;7612	15		.
	ret nz			;7613	c0		.
	ret nz			;7614	c0		.
	ret nz			;7615	c0		.
	ret nz			;7616	c0		.
	ret nz			;7617	c0		.
	ret nz			;7618	c0		.
	ret nz			;7619	c0		.
	ret nz			;761a	c0		.
	ret nz			;761b	c0		.
	ret nz			;761c	c0		.
	ret nz			;761d	c0		.
	dec d			;761e	15		.
	dec d			;761f	15		.
	dec d			;7620	15		.
	dec d			;7621	15		.
	dec d			;7622	15		.
	ret nz			;7623	c0		.
	ret nz			;7624	c0		.
	ret nz			;7625	c0		.
	ret nz			;7626	c0		.
	ret nz			;7627	c0		.
	ret nz			;7628	c0		.
	ret nz			;7629	c0		.
	ret nz			;762a	c0		.
	ret nz			;762b	c0		.
	ret nz			;762c	c0		.
	dec d			;762d	15		.
	dec d			;762e	15		.
	rlca			;762f	07		.
	dec d			;7630	15		.
	dec d			;7631	15		.
	dec d			;7632	15		.
	ret nz			;7633	c0		.
	ret nz			;7634	c0		.
	ret nz			;7635	c0		.
	ret nz			;7636	c0		.
	ret nz			;7637	c0		.
	ret nz			;7638	c0		.
	ret nz			;7639	c0		.
	ret nz			;763a	c0		.
	dec d			;763b	15		.
	dec d			;763c	15		.
	dec d			;763d	15		.
	rlca			;763e	07		.
	dec d			;763f	15		.
	dec d			;7640	15		.
	ret nz			;7641	c0		.
	ret nz			;7642	c0		.
	ret nz			;7643	c0		.
	ret nz			;7644	c0		.
	ret nz			;7645	c0		.
	ret nz			;7646	c0		.
	ret nz			;7647	c0		.
	ret nz			;7648	c0		.
	ret nz			;7649	c0		.
	dec d			;764a	15		.
	dec d			;764b	15		.
	dec d			;764c	15		.
	dec d			;764d	15		.
	dec d			;764e	15		.
	ret nz			;764f	c0		.
	ret nz			;7650	c0		.
	add hl,bc		;7651	09		.
	ret nz			;7652	c0		.
	add hl,bc		;7653	09		.
	ret nz			;7654	c0		.
	add hl,bc		;7655	09		.
	ret nz			;7656	c0		.
	add hl,bc		;7657	09		.
	ret nz			;7658	c0		.
	dec d			;7659	15		.
	dec d			;765a	15		.
	dec d			;765b	15		.
	dec d			;765c	15		.
	ret nz			;765d	c0		.
	ret nz			;765e	c0		.
	ret nz			;765f	c0		.
	add hl,bc		;7660	09		.
	ret nz			;7661	c0		.
	add hl,bc		;7662	09		.
	ret nz			;7663	c0		.
	add hl,bc		;7664	09		.
	ret nz			;7665	c0		.
	add hl,bc		;7666	09		.
	ret nz			;7667	c0		.
	dec d			;7668	15		.
	dec d			;7669	15		.
	dec d			;766a	15		.
	dec d			;766b	15		.
	dec d			;766c	15		.
	ret nz			;766d	c0		.
	ret nz			;766e	c0		.
	ret nz			;766f	c0		.
	ret nz			;7670	c0		.
	ret nz			;7671	c0		.
	ret nz			;7672	c0		.
	ret nz			;7673	c0		.
	ret nz			;7674	c0		.
	ret nz			;7675	c0		.
	ret nz			;7676	c0		.
	dec d			;7677	15		.
	dec d			;7678	15		.
	dec d			;7679	15		.
	dec d			;767a	15		.
	dec d			;767b	15		.
	dec d			;767c	15		.
	ret nz			;767d	c0		.
	ret nz			;767e	c0		.
	ret nz			;767f	c0		.
	ret nz			;7680	c0		.
	ret nz			;7681	c0		.
	ret nz			;7682	c0		.
	ret nz			;7683	c0		.
	ret nz			;7684	c0		.
	ret nz			;7685	c0		.
	ret nz			;7686	c0		.
	dec d			;7687	15		.
	dec d			;7688	15		.
	dec d			;7689	15		.
	dec d			;768a	15		.
	dec d			;768b	15		.
	dec d			;768c	15		.
	ret nz			;768d	c0		.
	ret nz			;768e	c0		.
	ret nz			;768f	c0		.
	ret nz			;7690	c0		.
	ret nz			;7691	c0		.
	ret nz			;7692	c0		.
	ret nz			;7693	c0		.
	ret nz			;7694	c0		.
	ret nz			;7695	c0		.
	dec d			;7696	15		.
	dec d			;7697	15		.
	dec d			;7698	15		.
	dec d			;7699	15		.
	dec d			;769a	15		.
	ret nz			;769b	c0		.
	ret nz			;769c	c0		.
	ret nz			;769d	c0		.
	ret nz			;769e	c0		.
	ret nz			;769f	c0		.
	ret nz			;76a0	c0		.
	ret nz			;76a1	c0		.
	ret nz			;76a2	c0		.
	ret nz			;76a3	c0		.
	ret nz			;76a4	c0		.
	ret nz			;76a5	c0		.
	dec d			;76a6	15		.
	dec d			;76a7	15		.
	dec d			;76a8	15		.
	ret nz			;76a9	c0		.
	ret nz			;76aa	c0		.
	ret nz			;76ab	c0		.
	ret nz			;76ac	c0		.
	ret nz			;76ad	c0		.
	ret nz			;76ae	c0		.
	ret nz			;76af	c0		.
	ret nz			;76b0	c0		.
	ret nz			;76b1	c0		.
	ret nz			;76b2	c0		.
	add hl,bc		;76b3	09		.
	add hl,bc		;76b4	09		.
	add hl,bc		;76b5	09		.
	ret nz			;76b6	c0		.
	rlca			;76b7	07		.
	ret nz			;76b8	c0		.
	ret nz			;76b9	c0		.
	ex af,af'		;76ba	08		.
	ret nz			;76bb	c0		.
	rlca			;76bc	07		.
	rlca			;76bd	07		.
	rlca			;76be	07		.
	ret nz			;76bf	c0		.
	add hl,bc		;76c0	09		.
	add hl,bc		;76c1	09		.
	add hl,bc		;76c2	09		.
	add hl,bc		;76c3	09		.
	add hl,bc		;76c4	09		.
	ret nz			;76c5	c0		.
	rlca			;76c6	07		.
	ret nz			;76c7	c0		.
	ret nz			;76c8	c0		.
	ex af,af'		;76c9	08		.
	ret nz			;76ca	c0		.
	rlca			;76cb	07		.
	rlca			;76cc	07		.
	rlca			;76cd	07		.
	ret nz			;76ce	c0		.
	add hl,bc		;76cf	09		.
	add hl,bc		;76d0	09		.
	add hl,bc		;76d1	09		.
	ret nz			;76d2	c0		.
	ret nz			;76d3	c0		.
	ret nz			;76d4	c0		.
	rlca			;76d5	07		.
	ret nz			;76d6	c0		.
	ret nz			;76d7	c0		.
	ex af,af'		;76d8	08		.
	ret nz			;76d9	c0		.
	ret nz			;76da	c0		.
	rlca			;76db	07		.
	ret nz			;76dc	c0		.
	ret nz			;76dd	c0		.
	add hl,bc		;76de	09		.
	ret nz			;76df	c0		.
	add hl,bc		;76e0	09		.
	ret nz			;76e1	c0		.
	ret nz			;76e2	c0		.
	ret nz			;76e3	c0		.
	rlca			;76e4	07		.
	ret nz			;76e5	c0		.
	ret nz			;76e6	c0		.
	ex af,af'		;76e7	08		.
	ret nz			;76e8	c0		.
	ret nz			;76e9	c0		.
	rlca			;76ea	07		.
	ret nz			;76eb	c0		.
	ret nz			;76ec	c0		.
	add hl,bc		;76ed	09		.
	ret nz			;76ee	c0		.
	add hl,bc		;76ef	09		.
	ret nz			;76f0	c0		.
	ret nz			;76f1	c0		.
	ret nz			;76f2	c0		.
	rlca			;76f3	07		.
	ret nz			;76f4	c0		.
	ret nz			;76f5	c0		.
	ex af,af'		;76f6	08		.
	ret nz			;76f7	c0		.
	ret nz			;76f8	c0		.
	rlca			;76f9	07		.
	ret nz			;76fa	c0		.
	ret nz			;76fb	c0		.
	add hl,bc		;76fc	09		.
	ret nz			;76fd	c0		.
	add hl,bc		;76fe	09		.
	add hl,bc		;76ff	09		.
	add hl,bc		;7700	09		.
	ret nz			;7701	c0		.
	rlca			;7702	07		.
	ret nz			;7703	c0		.
	ret nz			;7704	c0		.
	ex af,af'		;7705	08		.
	ret nz			;7706	c0		.
	ret nz			;7707	c0		.
	rlca			;7708	07		.
	ret nz			;7709	c0		.
	ret nz			;770a	c0		.
	add hl,bc		;770b	09		.
	add hl,bc		;770c	09		.
	add hl,bc		;770d	09		.
	add hl,bc		;770e	09		.
	add hl,bc		;770f	09		.
	ret nz			;7710	c0		.
	rlca			;7711	07		.
	ret nz			;7712	c0		.
	ret nz			;7713	c0		.
	ex af,af'		;7714	08		.
	ret nz			;7715	c0		.
	ret nz			;7716	c0		.
	rlca			;7717	07		.
	ret nz			;7718	c0		.
	ret nz			;7719	c0		.
	add hl,bc		;771a	09		.
	add hl,bc		;771b	09		.
	add hl,bc		;771c	09		.
	ret nz			;771d	c0		.
	ret nz			;771e	c0		.
	ret nz			;771f	c0		.
	rlca			;7720	07		.
	ret nz			;7721	c0		.
	ret nz			;7722	c0		.
	ex af,af'		;7723	08		.
	ret nz			;7724	c0		.
	ret nz			;7725	c0		.
	rlca			;7726	07		.
	ret nz			;7727	c0		.
	ret nz			;7728	c0		.
	add hl,bc		;7729	09		.
	ret nz			;772a	c0		.
	add hl,bc		;772b	09		.
	ret nz			;772c	c0		.
	ret nz			;772d	c0		.
	ret nz			;772e	c0		.
	rlca			;772f	07		.
	ret nz			;7730	c0		.
	ret nz			;7731	c0		.
	ex af,af'		;7732	08		.
	ret nz			;7733	c0		.
	ret nz			;7734	c0		.
	rlca			;7735	07		.
	ret nz			;7736	c0		.
	ret nz			;7737	c0		.
	add hl,bc		;7738	09		.
	ret nz			;7739	c0		.
	add hl,bc		;773a	09		.
	ret nz			;773b	c0		.
	ret nz			;773c	c0		.
	ret nz			;773d	c0		.
	rlca			;773e	07		.
	ret nz			;773f	c0		.
	ret nz			;7740	c0		.
	ex af,af'		;7741	08		.
	ret nz			;7742	c0		.
	ret nz			;7743	c0		.
	rlca			;7744	07		.
	ret nz			;7745	c0		.
	ret nz			;7746	c0		.
	add hl,bc		;7747	09		.
	ret nz			;7748	c0		.
	add hl,bc		;7749	09		.
	add hl,bc		;774a	09		.
	add hl,bc		;774b	09		.
	ret nz			;774c	c0		.
	rlca			;774d	07		.
	rlca			;774e	07		.
	ret nz			;774f	c0		.
	ex af,af'		;7750	08		.
	ret nz			;7751	c0		.
	ret nz			;7752	c0		.
	rlca			;7753	07		.
	ret nz			;7754	c0		.
	ret nz			;7755	c0		.
	add hl,bc		;7756	09		.
	add hl,bc		;7757	09		.
	add hl,bc		;7758	09		.
	add hl,bc		;7759	09		.
	add hl,bc		;775a	09		.
	ret nz			;775b	c0		.
	rlca			;775c	07		.
	rlca			;775d	07		.
	ret nz			;775e	c0		.
	ex af,af'		;775f	08		.
	ret nz			;7760	c0		.
	ret nz			;7761	c0		.
	rlca			;7762	07		.
	ret nz			;7763	c0		.
	ret nz			;7764	c0		.
	add hl,bc		;7765	09		.
	add hl,bc		;7766	09		.
sub_7767h:
	ld hl,l777eh		;7767	21 7e 77	! ~ w
	ld e,(ix+000h)		;776a	dd 5e 00	. ^ .
	sla e			;776d	cb 23		. #
	ld d,000h		;776f	16 00		. .
	add hl,de		;7771	19		.
	ld a,(hl)		;7772	7e		~
	inc hl			;7773	23		#
	ld h,(hl)		;7774	66		f
	ld l,a			;7775	6f		o
	ld e,(ix+001h)		;7776	dd 5e 01	. ^ .
	sla e			;7779	cb 23		. #
	add hl,de		;777b	19		.
	ld e,(hl)		;777c	5e		^
	inc hl			;777d	23		#
l777eh:
	ld d,(hl)		;777e	56		V
	ret			;777f	c9		.
	sub (hl)		;7780	96		.
	ld (hl),a		;7781	77		w
	cp (hl)			;7782	be		.
	ld (hl),a		;7783	77		w
	ld l,078h		;7784	2e 78		. x
	ret nc			;7786	d0		.
	ld (hl),a		;7787	77		w
	and 077h		;7788	e6 77		. w
	jp p,0f877h		;778a	f2 77 f8	. w .
	ld (hl),a		;778d	77		w
	ld d,(hl)		;778e	56		V
	ld a,b			;778f	78		x
	ld c,078h		;7790	0e 78		. x
	ld b,d			;7792	42		B
	ld a,b			;7793	78		x
	ld a,078h		;7794	3e 78		> x
l7796h:
	jr c,$+128		;7796	38 7e		8 ~
	ld c,(hl)		;7798	4e		N
	ld a,l			;7799	7d		}
	jp p,la27eh		;779a	f2 7e a2	. ~ .
	ld a,(hl)		;779d	7e		~
	ld b,d			;779e	42		B
	ld a,a			;779f	7f		.
	adc a,d			;77a0	8a		.
	add a,d			;77a1	82		.
	ret po			;77a2	e0		.
	ld a,a			;77a3	7f		.
	ld c,d			;77a4	4a		J
	add a,b			;77a5	80		.
	or h			;77a6	b4		.
	add a,b			;77a7	80		.
	ld e,081h		;77a8	1e 81		. .
	adc a,b			;77aa	88		.
	add a,c			;77ab	81		.
	jp p,01e81h		;77ac	f2 81 1e	. . .
	add a,c			;77af	81		.
	or h			;77b0	b4		.
	add a,b			;77b1	80		.
	ld c,d			;77b2	4a		J
	add a,b			;77b3	80		.
	ret po			;77b4	e0		.
	ld a,a			;77b5	7f		.
	ret po			;77b6	e0		.
	ld a,a			;77b7	7f		.
	ld c,d			;77b8	4a		J
	add a,b			;77b9	80		.
	or h			;77ba	b4		.
	add a,b			;77bb	80		.
	ld e,081h		;77bc	1e 81		. .
	ld d,07bh		;77be	16 7b		. {
	ld c,b			;77c0	48		H
	ld a,e			;77c1	7b		{
	sub d			;77c2	92		.
	ld a,e			;77c3	7b		{
	call c,0267bh		;77c4	dc 7b 26	. { &
	ld a,h			;77c7	7c		|
	ld (hl),b		;77c8	70		p
	ld a,h			;77c9	7c		|
	cp d			;77ca	ba		.
	ld a,h			;77cb	7c		|
	inc b			;77cc	04		.
	ld a,l			;77cd	7d		}
	adc a,h			;77ce	8c		.
	ld a,d			;77cf	7a		z
	ld l,h			;77d0	6c		l
	adc a,e			;77d1	8b		.
	inc c			;77d2	0c		.
	adc a,h			;77d3	8c		.
	jp pe,0228ch		;77d4	ea 8c 22	. . "
	adc a,e			;77d7	8b		.
	or b			;77d8	b0		.
	adc a,e			;77d9	8b		.
	ld b,h			;77da	44		D
	adc a,h			;77db	8c		.
	inc e			;77dc	1c		.
	adc a,c			;77dd	89		.
	ld l,d			;77de	6a		j
	adc a,d			;77df	8a		.
	sub h			;77e0	94		.
	adc a,h			;77e1	8c		.
	add a,08ah		;77e2	c6 8a		. .
	ld l,d			;77e4	6a		j
	ld a,b			;77e5	78		x
	jp nc,0e47dh		;77e6	d2 7d e4	. } .
	ld a,l			;77e9	7d		}
	or 07dh			;77ea	f6 7d		. }
	inc b			;77ec	04		.
	ld a,(hl)		;77ed	7e		~
	inc d			;77ee	14		.
	ld a,(hl)		;77ef	7e		~
	ld h,07eh		;77f0	26 7e		& ~
	inc e			;77f2	1c		.
	adc a,c			;77f3	89		.
	ret nz			;77f4	c0		.
	adc a,c			;77f5	89		.
	inc e			;77f6	1c		.
	adc a,c			;77f7	89		.
	ld b,d			;77f8	42		B
	add a,e			;77f9	83		.
	ld (hl),b		;77fa	70		p
	add a,e			;77fb	83		.
	add a,(hl)		;77fc	86		.
	add a,e			;77fd	83		.
	sbc a,b			;77fe	98		.
	add a,e			;77ff	83		.
sub_7800h:
	and (hl)		;7800	a6		.
	add a,e			;7801	83		.
	or b			;7802	b0		.
	add a,e			;7803	83		.
	ld b,084h		;7804	06 84		. .
	ld h,d			;7806	62		b
	add a,h			;7807	84		.
	call nz,02c84h		;7808	c4 84 2c	. . ,
	add a,l			;780b	85		.
	sbc a,d			;780c	9a		.
	add a,l			;780d	85		.
	ld c,086h		;780e	0e 86		. .
	ld l,d			;7810	6a		j
	add a,(hl)		;7811	86		.
	add a,086h		;7812	c6 86		. .
	ld (0c687h),hl		;7814	22 87 c6	" . .
	add a,(hl)		;7817	86		.
	ld l,d			;7818	6a		j
	add a,(hl)		;7819	86		.
	ld c,086h		;781a	0e 86		. .
	ld a,b			;781c	78		x
	add a,a			;781d	87		.
	ld c,086h		;781e	0e 86		. .
	ld l,d			;7820	6a		j
	add a,(hl)		;7821	86		.
	add a,086h		;7822	c6 86		. .
	and 087h		;7824	e6 87		. .
	inc e			;7826	1c		.
	adc a,b			;7827	88		.
	ld d,d			;7828	52		R
	adc a,b			;7829	88		.
	adc a,h			;782a	8c		.
	adc a,b			;782b	88		.
	adc a,088h		;782c	ce 88		. .
	call m,0ed7ah		;782e	fc 7a ed	. z .
	ld l,b			;7831	68		h
	rra			;7832	1f		.
	ld l,c			;7833	69		i
	ld d,c			;7834	51		Q
	ld l,c			;7835	69		i
	ld (hl),e		;7836	73		s
	ld l,c			;7837	69		i
	ld hl,(0387ah)		;7838	2a 7a 38	* z 8
	ld a,c			;783b	79		y
	xor h			;783c	ac		.
	ld a,b			;783d	78		x
	cp (hl)			;783e	be		.
	ld a,d			;783f	7a		z
	nop			;7840	00		.
	nop			;7841	00		.
	and 087h		;7842	e6 87		. .
	inc e			;7844	1c		.
	adc a,b			;7845	88		.
	ld d,d			;7846	52		R
	adc a,b			;7847	88		.
	adc a,h			;7848	8c		.
	adc a,b			;7849	88		.
	adc a,088h		;784a	ce 88		. .
	adc a,h			;784c	8c		.
	adc a,b			;784d	88		.
	ld d,d			;784e	52		R
	adc a,b			;784f	88		.
	inc e			;7850	1c		.
	adc a,b			;7851	88		.
	and 087h		;7852	e6 87		. .
	and 087h		;7854	e6 87		. .
	or b			;7856	b0		.
	add a,e			;7857	83		.
	ld b,084h		;7858	06 84		. .
	ld h,d			;785a	62		b
	add a,h			;785b	84		.
	call nz,02c84h		;785c	c4 84 2c	. . ,
	add a,l			;785f	85		.
	sbc a,d			;7860	9a		.
	add a,l			;7861	85		.
	inc l			;7862	2c		,
	add a,l			;7863	85		.
	call nz,06284h		;7864	c4 84 62	. . b
	add a,h			;7867	84		.
	ld b,084h		;7868	06 84		. .
	ld (bc),a		;786a	02		.
	djnz l78e9h		;786b	10 7c		. |
	nop			;786d	00		.
	nop			;786e	00		.
	nop			;786f	00		.
	cp 05ch			;7870	fe 5c		. \
	nop			;7872	00		.
	nop			;7873	00		.
	ld a,h			;7874	7c		|
	jr c,l7877h		;7875	38 00		8 .
l7877h:
	nop			;7877	00		.
	jr c,l787ah		;7878	38 00		8 .
l787ah:
	nop			;787a	00		.
	nop			;787b	00		.
	ld a,h			;787c	7c		|
	jr c,l787fh		;787d	38 00		8 .
l787fh:
	nop			;787f	00		.
	cp 05ch			;7880	fe 5c		. \
	nop			;7882	00		.
	nop			;7883	00		.
	cp 05ch			;7884	fe 5c		. \
	nop			;7886	00		.
	nop			;7887	00		.
	cp 05ch			;7888	fe 5c		. \
	ret m			;788a	f8		.
	nop			;788b	00		.
	cp 05ch			;788c	fe 5c		. \
	ld (hl),b		;788e	70		p
	nop			;788f	00		.
	ld a,h			;7890	7c		|
	jr c,l7893h		;7891	38 00		8 .
l7893h:
	nop			;7893	00		.
	jr c,l7896h		;7894	38 00		8 .
l7896h:
	ld (hl),b		;7896	70		p
	nop			;7897	00		.
	nop			;7898	00		.
	nop			;7899	00		.
	ret m			;789a	f8		.
	nop			;789b	00		.
	nop			;789c	00		.
	nop			;789d	00		.
	ret m			;789e	f8		.
	nop			;789f	00		.
	nop			;78a0	00		.
	nop			;78a1	00		.
	ret m			;78a2	f8		.
	nop			;78a3	00		.
	nop			;78a4	00		.
	nop			;78a5	00		.
	ret m			;78a6	f8		.
	nop			;78a7	00		.
	nop			;78a8	00		.
	nop			;78a9	00		.
	ld (hl),b		;78aa	70		p
	nop			;78ab	00		.
	inc bc			;78ac	03		.
	rla			;78ad	17		.
	nop			;78ae	00		.
	nop			;78af	00		.
	rst 38h			;78b0	ff		.
	nop			;78b1	00		.
	add a,b			;78b2	80		.
	nop			;78b3	00		.
	rlca			;78b4	07		.
	nop			;78b5	00		.
	rst 38h			;78b6	ff		.
	ld a,a			;78b7	7f		.
	ret p			;78b8	f0		.
	nop			;78b9	00		.
	rra			;78ba	1f		.
	inc bc			;78bb	03		.
	rst 38h			;78bc	ff		.
	pop bc			;78bd	c1		.
	call m,03fe0h		;78be	fc e0 3f	. . ?
	ld c,0ffh		;78c1	0e ff		. .
	inc e			;78c3	1c		.
	cp 038h			;78c4	fe 38		. 8
	ccf			;78c6	3f		?
	jr $+1			;78c7	18 ff		. .
l78c9h:
	djnz l78c9h		;78c9	10 fe		. .
	inc c			;78cb	0c		.
	ld a,a			;78cc	7f		.
	ld de,018ffh		;78cd	11 ff 18	. . .
	rst 38h			;78d0	ff		.
	ld b,h			;78d1	44		D
	ld a,a			;78d2	7f		.
	ld (l90ffh),a		;78d3	32 ff 90	2 . .
	rst 38h			;78d6	ff		.
	and (hl)		;78d7	a6		.
	ld a,a			;78d8	7f		.
	inc h			;78d9	24		$
	rst 38h			;78da	ff		.
	ld b,c			;78db	41		A
	rst 38h			;78dc	ff		.
	ld d,d			;78dd	52		R
	rst 38h			;78de	ff		.
	ld (l9cffh),hl		;78df	22 ff 9c	" . .
	rst 38h			;78e2	ff		.
	ld (bc),a		;78e3	02		.
	rst 38h			;78e4	ff		.
	ld h,c			;78e5	61		a
	rst 38h			;78e6	ff		.
	ld h,0ffh		;78e7	26 ff		& .
l78e9h:
	inc bc			;78e9	03		.
	rst 38h			;78ea	ff		.
	ld b,b			;78eb	40		@
	rst 38h			;78ec	ff		.
	ld c,a			;78ed	4f		O
	rst 38h			;78ee	ff		.
	ld bc,04cffh		;78ef	01 ff 4c	. . L
	rst 38h			;78f2	ff		.
	ld e,a			;78f3	5f		_
	rst 38h			;78f4	ff		.
	add hl,de		;78f5	19		.
	rst 38h			;78f6	ff		.
	ld c,h			;78f7	4c		L
	rst 38h			;78f8	ff		.
	ld e,a			;78f9	5f		_
	rst 38h			;78fa	ff		.
	add hl,de		;78fb	19		.
	rst 38h			;78fc	ff		.
	ld b,b			;78fd	40		@
	rst 38h			;78fe	ff		.
	ld a,a			;78ff	7f		.
	rst 38h			;7900	ff		.
	ld bc,060ffh		;7901	01 ff 60	. . `
	rst 38h			;7904	ff		.
	ld a,0ffh		;7905	3e ff		> .
	inc bc			;7907	03		.
	rst 38h			;7908	ff		.
	ld hl,01cffh		;7909	21 ff 1c	! . .
	rst 38h			;790c	ff		.
	ld b,d			;790d	42		B
	ld a,a			;790e	7f		.
	ld (l80ffh),hl		;790f	22 ff 80	" . .
	rst 38h			;7912	ff		.
	add a,d			;7913	82		.
	ld a,a			;7914	7f		.
	inc (hl)		;7915	34		4
	rst 38h			;7916	ff		.
	ld e,l			;7917	5d		]
	rst 38h			;7918	ff		.
	ld b,(hl)		;7919	46		F
	ld a,a			;791a	7f		.
	ld (de),a		;791b	12		.
	rst 38h			;791c	ff		.
	sub b			;791d	90		.
	rst 38h			;791e	ff		.
	add a,h			;791f	84		.
	ccf			;7920	3f		?
	add hl,de		;7921	19		.
	rst 38h			;7922	ff		.
	jr $+1			;7923	18 ff		. .
	ld c,h			;7925	4c		L
	ccf			;7926	3f		?
	ld c,0ffh		;7927	0e ff		. .
	djnz $+1		;7929	10 ff		. .
	jr c,l794ch		;792b	38 1f		8 .
	inc bc			;792d	03		.
	rst 38h			;792e	ff		.
	pop bc			;792f	c1		.
	cp 0e0h			;7930	fe e0		. .
	rlca			;7932	07		.
	nop			;7933	00		.
	rst 38h			;7934	ff		.
	ld a,a			;7935	7f		.
	push af			;7936	f5		.
	nop			;7937	00		.
	inc b			;7938	04		.
l7939h:
	ld e,000h		;7939	1e 00		. .
	nop			;793b	00		.
	rst 38h			;793c	ff		.
	nop			;793d	00		.
	add a,b			;793e	80		.
	nop			;793f	00		.
	nop			;7940	00		.
	nop			;7941	00		.
	rlca			;7942	07		.
	nop			;7943	00		.
	rst 38h			;7944	ff		.
	ld a,a			;7945	7f		.
	ret p			;7946	f0		.
	nop			;7947	00		.
	nop			;7948	00		.
	nop			;7949	00		.
	rra			;794a	1f		.
	inc bc			;794b	03		.
l794ch:
	rst 38h			;794c	ff		.
	rst 30h			;794d	f7		.
	call m,000e0h		;794e	fc e0 00	. . .
	nop			;7951	00		.
	ccf			;7952	3f		?
	rrca			;7953	0f		.
	rst 38h			;7954	ff		.
	rst 30h			;7955	f7		.
	cp 0f8h			;7956	fe f8		. .
	nop			;7958	00		.
	nop			;7959	00		.
	ccf			;795a	3f		?
	rra			;795b	1f		.
	rst 38h			;795c	ff		.
	rst 30h			;795d	f7		.
	cp 0fch			;795e	fe fc		. .
	nop			;7960	00		.
	nop			;7961	00		.
	ld a,a			;7962	7f		.
	rra			;7963	1f		.
	rst 38h			;7964	ff		.
	rst 30h			;7965	f7		.
	rst 38h			;7966	ff		.
	call m,00000h		;7967	fc 00 00	. . .
	ld a,a			;796a	7f		.
	daa			;796b	27		'
	rst 38h			;796c	ff		.
	pop bc			;796d	c1		.
	rst 38h			;796e	ff		.
	jp p,00000h		;796f	f2 00 00	. . .
	ld a,a			;7972	7f		.
	add hl,sp		;7973	39		9
	rst 38h			;7974	ff		.
	ex (sp),hl		;7975	e3		.
	rst 38h			;7976	ff		.
	adc a,000h		;7977	ce 00		. .
	nop			;7979	00		.
	rst 38h			;797a	ff		.
	ld a,0ffh		;797b	3e ff		> .
	or (hl)			;797d	b6		.
	rst 38h			;797e	ff		.
	cp (hl)			;797f	be		.
	add a,b			;7980	80		.
	nop			;7981	00		.
	rst 38h			;7982	ff		.
	ld a,a			;7983	7f		.
	rst 38h			;7984	ff		.
	ld a,0ffh		;7985	3e ff		> .
	ld a,a			;7987	7f		.
	add a,b			;7988	80		.
	nop			;7989	00		.
	rst 38h			;798a	ff		.
	ld a,(hl)		;798b	7e		~
	rst 38h			;798c	ff		.
	ld a,0ffh		;798d	3e ff		> .
	ccf			;798f	3f		?
	ret nz			;7990	c0		.
	nop			;7991	00		.
	rst 38h			;7992	ff		.
	ld a,a			;7993	7f		.
	rst 38h			;7994	ff		.
	rst 38h			;7995	ff		.
	rst 38h			;7996	ff		.
	rst 38h			;7997	ff		.
	and b			;7998	a0		.
	nop			;7999	00		.
	rst 38h			;799a	ff		.
	ld a,a			;799b	7f		.
	rst 38h			;799c	ff		.
	rst 38h			;799d	ff		.
	rst 38h			;799e	ff		.
	rst 38h			;799f	ff		.
	ret nc			;79a0	d0		.
	nop			;79a1	00		.
	rst 38h			;79a2	ff		.
	ld a,(hl)		;79a3	7e		~
	rst 38h			;79a4	ff		.
	ld a,0ffh		;79a5	3e ff		> .
	ccf			;79a7	3f		?
	xor b			;79a8	a8		.
	nop			;79a9	00		.
	rst 38h			;79aa	ff		.
	ld a,a			;79ab	7f		.
	rst 38h			;79ac	ff		.
	ld a,0ffh		;79ad	3e ff		> .
	ld a,a			;79af	7f		.
	ret nc			;79b0	d0		.
	nop			;79b1	00		.
	rst 38h			;79b2	ff		.
	ld a,0ffh		;79b3	3e ff		> .
	or (hl)			;79b5	b6		.
	rst 38h			;79b6	ff		.
	cp (hl)			;79b7	be		.
	xor b			;79b8	a8		.
	nop			;79b9	00		.
	ld a,a			;79ba	7f		.
	add hl,sp		;79bb	39		9
	rst 38h			;79bc	ff		.
	ex (sp),hl		;79bd	e3		.
	rst 38h			;79be	ff		.
	adc a,054h		;79bf	ce 54		. T
	nop			;79c1	00		.
	ld a,a			;79c2	7f		.
	daa			;79c3	27		'
	rst 38h			;79c4	ff		.
	pop bc			;79c5	c1		.
	rst 38h			;79c6	ff		.
	jp p,000a8h		;79c7	f2 a8 00	. . .
	ld a,a			;79ca	7f		.
	rra			;79cb	1f		.
	rst 38h			;79cc	ff		.
	rst 30h			;79cd	f7		.
	rst 38h			;79ce	ff		.
	call m,00054h		;79cf	fc 54 00	. T .
	ccf			;79d2	3f		?
	rra			;79d3	1f		.
	rst 38h			;79d4	ff		.
	rst 30h			;79d5	f7		.
	rst 38h			;79d6	ff		.
	call m,000a8h		;79d7	fc a8 00	. . .
	ccf			;79da	3f		?
	rrca			;79db	0f		.
	rst 38h			;79dc	ff		.
	rst 30h			;79dd	f7		.
	rst 38h			;79de	ff		.
	ret m			;79df	f8		.
	ld d,h			;79e0	54		T
	nop			;79e1	00		.
	rra			;79e2	1f		.
	inc bc			;79e3	03		.
	rst 38h			;79e4	ff		.
	rst 30h			;79e5	f7		.
	cp 0e0h			;79e6	fe e0		. .
	xor b			;79e8	a8		.
	nop			;79e9	00		.
	rlca			;79ea	07		.
	nop			;79eb	00		.
	rst 38h			;79ec	ff		.
	ld a,a			;79ed	7f		.
	push af			;79ee	f5		.
	nop			;79ef	00		.
	ld d,b			;79f0	50		P
	nop			;79f1	00		.
	nop			;79f2	00		.
	nop			;79f3	00		.
	rst 38h			;79f4	ff		.
	nop			;79f5	00		.
	xor d			;79f6	aa		.
	nop			;79f7	00		.
	xor b			;79f8	a8		.
	nop			;79f9	00		.
	nop			;79fa	00		.
	nop			;79fb	00		.
	ld d,l			;79fc	55		U
	nop			;79fd	00		.
	ld d,l			;79fe	55		U
	nop			;79ff	00		.
	ld d,b			;7a00	50		P
	nop			;7a01	00		.
	nop			;7a02	00		.
	nop			;7a03	00		.
	xor d			;7a04	aa		.
	nop			;7a05	00		.
	xor d			;7a06	aa		.
	nop			;7a07	00		.
	and b			;7a08	a0		.
	nop			;7a09	00		.
	nop			;7a0a	00		.
	nop			;7a0b	00		.
	ld d,l			;7a0c	55		U
	nop			;7a0d	00		.
	ld d,l			;7a0e	55		U
	nop			;7a0f	00		.
	ld d,b			;7a10	50		P
	nop			;7a11	00		.
	nop			;7a12	00		.
	nop			;7a13	00		.
	ld hl,(la9ffh+1)	;7a14	2a 00 aa	* . .
	nop			;7a17	00		.
	and b			;7a18	a0		.
	nop			;7a19	00		.
	nop			;7a1a	00		.
	nop			;7a1b	00		.
	dec b			;7a1c	05		.
	nop			;7a1d	00		.
	ld d,l			;7a1e	55		U
	nop			;7a1f	00		.
	ld b,b			;7a20	40		@
	nop			;7a21	00		.
	nop			;7a22	00		.
	nop			;7a23	00		.
	nop			;7a24	00		.
	nop			;7a25	00		.
	xor d			;7a26	aa		.
	nop			;7a27	00		.
	nop			;7a28	00		.
	nop			;7a29	00		.
	ld (bc),a		;7a2a	02		.
	jr $+50			;7a2b	18 30		. 0
	nop			;7a2d	00		.
	nop			;7a2e	00		.
	nop			;7a2f	00		.
	ld a,b			;7a30	78		x
	jr nc,l7a33h		;7a31	30 00		0 .
l7a33h:
	nop			;7a33	00		.
	call m,00058h		;7a34	fc 58 00	. X .
	nop			;7a37	00		.
	call m,00058h		;7a38	fc 58 00	. X .
	nop			;7a3b	00		.
	call m,00058h		;7a3c	fc 58 00	. X .
	nop			;7a3f	00		.
	call m,00000h		;7a40	fc 00 00	. . .
	nop			;7a43	00		.
	call m,00058h		;7a44	fc 58 00	. X .
	nop			;7a47	00		.
	call m,00058h		;7a48	fc 58 00	. X .
	nop			;7a4b	00		.
	call m,00058h		;7a4c	fc 58 00	. X .
	nop			;7a4f	00		.
	call m,06058h		;7a50	fc 58 60	. X `
	nop			;7a53	00		.
	call m,0f058h		;7a54	fc 58 f0	. X .
	nop			;7a57	00		.
	call m,0f000h		;7a58	fc 00 f0	. . .
l7a5bh:
	nop			;7a5b	00		.
	call m,0f058h		;7a5c	fc 58 f0	. X .
	nop			;7a5f	00		.
	call m,0f058h		;7a60	fc 58 f0	. X .
	nop			;7a63	00		.
	call m,0f058h		;7a64	fc 58 f0	. X .
	nop			;7a67	00		.
	ld a,b			;7a68	78		x
	jr nc,l7a5bh		;7a69	30 f0		0 .
	nop			;7a6b	00		.
	jr nc,l7a6eh		;7a6c	30 00		0 .
l7a6eh:
	ret p			;7a6e	f0		.
	nop			;7a6f	00		.
	nop			;7a70	00		.
	nop			;7a71	00		.
	ret p			;7a72	f0		.
	nop			;7a73	00		.
	nop			;7a74	00		.
	nop			;7a75	00		.
	ret p			;7a76	f0		.
	nop			;7a77	00		.
	nop			;7a78	00		.
	nop			;7a79	00		.
	ret p			;7a7a	f0		.
	nop			;7a7b	00		.
	nop			;7a7c	00		.
	nop			;7a7d	00		.
	ret p			;7a7e	f0		.
	nop			;7a7f	00		.
	nop			;7a80	00		.
	nop			;7a81	00		.
	ret p			;7a82	f0		.
	nop			;7a83	00		.
	nop			;7a84	00		.
	nop			;7a85	00		.
	ret p			;7a86	f0		.
	nop			;7a87	00		.
	nop			;7a88	00		.
	nop			;7a89	00		.
	ld h,b			;7a8a	60		`
	nop			;7a8b	00		.
	ld (bc),a		;7a8c	02		.
	inc c			;7a8d	0c		.
	inc e			;7a8e	1c		.
	nop			;7a8f	00		.
	nop			;7a90	00		.
	nop			;7a91	00		.
	ld a,01ch		;7a92	3e 1c		> .
	nop			;7a94	00		.
	nop			;7a95	00		.
	ld a,a			;7a96	7f		.
	ld h,000h		;7a97	26 00		& .
	nop			;7a99	00		.
	rst 38h			;7a9a	ff		.
	ld c,a			;7a9b	4f		O
	add a,b			;7a9c	80		.
	nop			;7a9d	00		.
	rst 38h			;7a9e	ff		.
	ld e,a			;7a9f	5f		_
	add a,b			;7aa0	80		.
	nop			;7aa1	00		.
	rst 38h			;7aa2	ff		.
	ld a,a			;7aa3	7f		.
	ret p			;7aa4	f0		.
	nop			;7aa5	00		.
	ld a,a			;7aa6	7f		.
	ld a,0f8h		;7aa7	3e f8		> .
	nop			;7aa9	00		.
	ccf			;7aaa	3f		?
	inc e			;7aab	1c		.
	call m,01d00h		;7aac	fc 00 1d	. . .
	nop			;7aaf	00		.
	call m,00000h		;7ab0	fc 00 00	. . .
	nop			;7ab3	00		.
	call m,00000h		;7ab4	fc 00 00	. . .
	nop			;7ab7	00		.
	ret m			;7ab8	f8		.
	nop			;7ab9	00		.
	nop			;7aba	00		.
	nop			;7abb	00		.
	ld (hl),b		;7abc	70		p
	nop			;7abd	00		.
	inc bc			;7abe	03		.
	ld a,(bc)		;7abf	0a		.
	jr nc,l7ac2h		;7ac0	30 00		0 .
l7ac2h:
	ld a,(hl)		;7ac2	7e		~
	nop			;7ac3	00		.
	call m,sub_7800h	;7ac4	fc 00 78	. . x
	jr nc,$+1		;7ac7	30 ff		0 .
	ld a,(hl)		;7ac9	7e		~
	cp 0fch			;7aca	fe fc		. .
	ld a,(hl)		;7acc	7e		~
	jr nc,$+1		;7acd	30 ff		0 .
	ld a,(hl)		;7acf	7e		~
	cp 0fch			;7ad0	fe fc		. .
	ld a,a			;7ad2	7f		.
	ld (hl),0ffh		;7ad3	36 ff		6 .
	ld h,(hl)		;7ad5	66		f
	cp 0cch			;7ad6	fe cc		. .
	ld a,a			;7ad8	7f		.
	ld (hl),0ffh		;7ad9	36 ff		6 .
	ld h,(hl)		;7adb	66		f
	cp 0cch			;7adc	fe cc		. .
	ld a,a			;7ade	7f		.
	ccf			;7adf	3f		?
	rst 38h			;7ae0	ff		.
	ld a,(hl)		;7ae1	7e		~
	cp 0fch			;7ae2	fe fc		. .
	ld a,a			;7ae4	7f		.
	ccf			;7ae5	3f		?
	rst 38h			;7ae6	ff		.
	ld a,(hl)		;7ae7	7e		~
	cp 0fch			;7ae8	fe fc		. .
	ccf			;7aea	3f		?
	ld b,07eh		;7aeb	06 7e		. ~
	nop			;7aed	00		.
	call m,00f00h		;7aee	fc 00 0f	. . .
	ld b,000h		;7af1	06 00		. .
	nop			;7af3	00		.
l7af4h:
	nop			;7af4	00		.
	nop			;7af5	00		.
	ld b,000h		;7af6	06 00		. .
	nop			;7af8	00		.
	nop			;7af9	00		.
	nop			;7afa	00		.
	nop			;7afb	00		.
	ld (bc),a		;7afc	02		.
	ld b,01fh		;7afd	06 1f		. .
	nop			;7aff	00		.
	cp 000h			;7b00	fe 00		. .
	ccf			;7b02	3f		?
	dec bc			;7b03	0b		.
	rst 38h			;7b04	ff		.
	call p,01b3fh		;7b05	f4 3f 1b	. ? .
	rst 38h			;7b08	ff		.
	or 03fh			;7b09	f6 3f		. ?
	dec de			;7b0b	1b		.
	rst 38h			;7b0c	ff		.
	or 03fh			;7b0d	f6 3f		. ?
	dec bc			;7b0f	0b		.
	rst 38h			;7b10	ff		.
	call p,0001fh		;7b11	f4 1f 00	. . .
	cp 000h			;7b14	fe 00		. .
	ld (bc),a		;7b16	02		.
	inc c			;7b17	0c		.
	jr c,l7b1ah		;7b18	38 00		8 .
l7b1ah:
	nop			;7b1a	00		.
	nop			;7b1b	00		.
	ld a,h			;7b1c	7c		|
	jr c,l7b1fh		;7b1d	38 00		8 .
l7b1fh:
	nop			;7b1f	00		.
	cp 04ch			;7b20	fe 4c		. L
	nop			;7b22	00		.
	nop			;7b23	00		.
	cp 05ch			;7b24	fe 5c		. \
	nop			;7b26	00		.
	nop			;7b27	00		.
	cp 07ch			;7b28	fe 7c		. |
	nop			;7b2a	00		.
	nop			;7b2b	00		.
	ld a,h			;7b2c	7c		|
	jr c,l7b2fh		;7b2d	38 00		8 .
l7b2fh:
	nop			;7b2f	00		.
	jr c,l7b32h		;7b30	38 00		8 .
l7b32h:
	nop			;7b32	00		.
	nop			;7b33	00		.
	nop			;7b34	00		.
	nop			;7b35	00		.
	ret po			;7b36	e0		.
	nop			;7b37	00		.
	ld bc,0f000h		;7b38	01 00 f0	. . .
	nop			;7b3b	00		.
	ld bc,0f000h		;7b3c	01 00 f0	. . .
	nop			;7b3f	00		.
	ld bc,0f000h		;7b40	01 00 f0	. . .
	nop			;7b43	00		.
	nop			;7b44	00		.
	nop			;7b45	00		.
	ret po			;7b46	e0		.
	nop			;7b47	00		.
	nop			;7b48	00		.
	nop			;7b49	00		.
	nop			;7b4a	00		.
	nop			;7b4b	00		.
	nop			;7b4c	00		.
	nop			;7b4d	00		.
	nop			;7b4e	00		.
	nop			;7b4f	00		.
	nop			;7b50	00		.
	nop			;7b51	00		.
	nop			;7b52	00		.
	nop			;7b53	00		.
	nop			;7b54	00		.
	nop			;7b55	00		.
	nop			;7b56	00		.
	nop			;7b57	00		.
	nop			;7b58	00		.
	nop			;7b59	00		.
	nop			;7b5a	00		.
	nop			;7b5b	00		.
	nop			;7b5c	00		.
	nop			;7b5d	00		.
	nop			;7b5e	00		.
	nop			;7b5f	00		.
	nop			;7b60	00		.
	nop			;7b61	00		.
	nop			;7b62	00		.
	nop			;7b63	00		.
	nop			;7b64	00		.
	nop			;7b65	00		.
	nop			;7b66	00		.
	nop			;7b67	00		.
	nop			;7b68	00		.
	nop			;7b69	00		.
	nop			;7b6a	00		.
	nop			;7b6b	00		.
	nop			;7b6c	00		.
	nop			;7b6d	00		.
	nop			;7b6e	00		.
	nop			;7b6f	00		.
	nop			;7b70	00		.
	nop			;7b71	00		.
	nop			;7b72	00		.
	nop			;7b73	00		.
	nop			;7b74	00		.
	nop			;7b75	00		.
	nop			;7b76	00		.
	nop			;7b77	00		.
	nop			;7b78	00		.
	nop			;7b79	00		.
	nop			;7b7a	00		.
	nop			;7b7b	00		.
	nop			;7b7c	00		.
	nop			;7b7d	00		.
	nop			;7b7e	00		.
	nop			;7b7f	00		.
	nop			;7b80	00		.
	nop			;7b81	00		.
	nop			;7b82	00		.
	nop			;7b83	00		.
	nop			;7b84	00		.
	nop			;7b85	00		.
	nop			;7b86	00		.
	nop			;7b87	00		.
	nop			;7b88	00		.
	nop			;7b89	00		.
	nop			;7b8a	00		.
	nop			;7b8b	00		.
	nop			;7b8c	00		.
	nop			;7b8d	00		.
	nop			;7b8e	00		.
	nop			;7b8f	00		.
	nop			;7b90	00		.
	nop			;7b91	00		.
	nop			;7b92	00		.
	nop			;7b93	00		.
	nop			;7b94	00		.
	nop			;7b95	00		.
	nop			;7b96	00		.
	nop			;7b97	00		.
	nop			;7b98	00		.
	nop			;7b99	00		.
	nop			;7b9a	00		.
	nop			;7b9b	00		.
	nop			;7b9c	00		.
	nop			;7b9d	00		.
	nop			;7b9e	00		.
	nop			;7b9f	00		.
	nop			;7ba0	00		.
	nop			;7ba1	00		.
	nop			;7ba2	00		.
	nop			;7ba3	00		.
	nop			;7ba4	00		.
	nop			;7ba5	00		.
	nop			;7ba6	00		.
	nop			;7ba7	00		.
	nop			;7ba8	00		.
	nop			;7ba9	00		.
	nop			;7baa	00		.
	nop			;7bab	00		.
	nop			;7bac	00		.
	nop			;7bad	00		.
	nop			;7bae	00		.
	nop			;7baf	00		.
	nop			;7bb0	00		.
	nop			;7bb1	00		.
	nop			;7bb2	00		.
	nop			;7bb3	00		.
	nop			;7bb4	00		.
	nop			;7bb5	00		.
	nop			;7bb6	00		.
	nop			;7bb7	00		.
	nop			;7bb8	00		.
	nop			;7bb9	00		.
	nop			;7bba	00		.
	nop			;7bbb	00		.
	nop			;7bbc	00		.
	nop			;7bbd	00		.
	nop			;7bbe	00		.
	nop			;7bbf	00		.
	nop			;7bc0	00		.
	nop			;7bc1	00		.
	nop			;7bc2	00		.
	nop			;7bc3	00		.
	nop			;7bc4	00		.
	nop			;7bc5	00		.
	nop			;7bc6	00		.
	nop			;7bc7	00		.
	nop			;7bc8	00		.
	nop			;7bc9	00		.
	nop			;7bca	00		.
	nop			;7bcb	00		.
	nop			;7bcc	00		.
	nop			;7bcd	00		.
	nop			;7bce	00		.
	nop			;7bcf	00		.
	nop			;7bd0	00		.
	nop			;7bd1	00		.
	nop			;7bd2	00		.
	nop			;7bd3	00		.
	nop			;7bd4	00		.
	nop			;7bd5	00		.
	nop			;7bd6	00		.
	nop			;7bd7	00		.
	nop			;7bd8	00		.
	nop			;7bd9	00		.
	nop			;7bda	00		.
	nop			;7bdb	00		.
	nop			;7bdc	00		.
	nop			;7bdd	00		.
	nop			;7bde	00		.
	nop			;7bdf	00		.
	nop			;7be0	00		.
	nop			;7be1	00		.
	nop			;7be2	00		.
	nop			;7be3	00		.
	nop			;7be4	00		.
	nop			;7be5	00		.
	nop			;7be6	00		.
	nop			;7be7	00		.
	nop			;7be8	00		.
	nop			;7be9	00		.
	nop			;7bea	00		.
	nop			;7beb	00		.
	nop			;7bec	00		.
	nop			;7bed	00		.
	nop			;7bee	00		.
	nop			;7bef	00		.
	nop			;7bf0	00		.
	nop			;7bf1	00		.
	nop			;7bf2	00		.
	nop			;7bf3	00		.
	nop			;7bf4	00		.
	nop			;7bf5	00		.
	nop			;7bf6	00		.
	nop			;7bf7	00		.
	nop			;7bf8	00		.
	nop			;7bf9	00		.
	nop			;7bfa	00		.
	nop			;7bfb	00		.
	nop			;7bfc	00		.
	nop			;7bfd	00		.
	nop			;7bfe	00		.
	nop			;7bff	00		.
sub_7c00h:
	nop			;7c00	00		.
	nop			;7c01	00		.
	nop			;7c02	00		.
	nop			;7c03	00		.
	nop			;7c04	00		.
	nop			;7c05	00		.
	nop			;7c06	00		.
	nop			;7c07	00		.
	nop			;7c08	00		.
	nop			;7c09	00		.
	nop			;7c0a	00		.
	nop			;7c0b	00		.
	nop			;7c0c	00		.
	nop			;7c0d	00		.
	nop			;7c0e	00		.
	nop			;7c0f	00		.
	nop			;7c10	00		.
	nop			;7c11	00		.
	nop			;7c12	00		.
	nop			;7c13	00		.
	nop			;7c14	00		.
	nop			;7c15	00		.
	nop			;7c16	00		.
	nop			;7c17	00		.
	nop			;7c18	00		.
	nop			;7c19	00		.
	nop			;7c1a	00		.
	nop			;7c1b	00		.
	nop			;7c1c	00		.
	nop			;7c1d	00		.
	nop			;7c1e	00		.
	nop			;7c1f	00		.
	nop			;7c20	00		.
	nop			;7c21	00		.
	nop			;7c22	00		.
	nop			;7c23	00		.
	nop			;7c24	00		.
	nop			;7c25	00		.
	nop			;7c26	00		.
	nop			;7c27	00		.
	nop			;7c28	00		.
	nop			;7c29	00		.
	nop			;7c2a	00		.
	nop			;7c2b	00		.
	nop			;7c2c	00		.
	nop			;7c2d	00		.
	nop			;7c2e	00		.
	nop			;7c2f	00		.
	nop			;7c30	00		.
	nop			;7c31	00		.
	nop			;7c32	00		.
	nop			;7c33	00		.
	nop			;7c34	00		.
	nop			;7c35	00		.
	nop			;7c36	00		.
	nop			;7c37	00		.
	nop			;7c38	00		.
	nop			;7c39	00		.
	nop			;7c3a	00		.
	nop			;7c3b	00		.
	nop			;7c3c	00		.
	nop			;7c3d	00		.
	nop			;7c3e	00		.
	nop			;7c3f	00		.
	nop			;7c40	00		.
	nop			;7c41	00		.
	nop			;7c42	00		.
	nop			;7c43	00		.
	nop			;7c44	00		.
	nop			;7c45	00		.
	nop			;7c46	00		.
	nop			;7c47	00		.
	nop			;7c48	00		.
	nop			;7c49	00		.
	nop			;7c4a	00		.
	nop			;7c4b	00		.
	nop			;7c4c	00		.
	nop			;7c4d	00		.
	nop			;7c4e	00		.
	nop			;7c4f	00		.
	nop			;7c50	00		.
	nop			;7c51	00		.
	nop			;7c52	00		.
	nop			;7c53	00		.
	nop			;7c54	00		.
	nop			;7c55	00		.
	nop			;7c56	00		.
	nop			;7c57	00		.
	nop			;7c58	00		.
	nop			;7c59	00		.
	nop			;7c5a	00		.
	nop			;7c5b	00		.
	nop			;7c5c	00		.
	nop			;7c5d	00		.
	nop			;7c5e	00		.
	nop			;7c5f	00		.
	nop			;7c60	00		.
	nop			;7c61	00		.
	nop			;7c62	00		.
	nop			;7c63	00		.
	nop			;7c64	00		.
	nop			;7c65	00		.
	nop			;7c66	00		.
	nop			;7c67	00		.
	nop			;7c68	00		.
	nop			;7c69	00		.
	nop			;7c6a	00		.
	nop			;7c6b	00		.
	nop			;7c6c	00		.
	nop			;7c6d	00		.
	nop			;7c6e	00		.
	nop			;7c6f	00		.
	nop			;7c70	00		.
	nop			;7c71	00		.
	nop			;7c72	00		.
	nop			;7c73	00		.
l7c74h:
	nop			;7c74	00		.
	nop			;7c75	00		.
	nop			;7c76	00		.
	nop			;7c77	00		.
	nop			;7c78	00		.
	nop			;7c79	00		.
	nop			;7c7a	00		.
	nop			;7c7b	00		.
	nop			;7c7c	00		.
	nop			;7c7d	00		.
	nop			;7c7e	00		.
	nop			;7c7f	00		.
	nop			;7c80	00		.
	nop			;7c81	00		.
	nop			;7c82	00		.
	nop			;7c83	00		.
	nop			;7c84	00		.
	nop			;7c85	00		.
	nop			;7c86	00		.
	nop			;7c87	00		.
	nop			;7c88	00		.
	nop			;7c89	00		.
	nop			;7c8a	00		.
	nop			;7c8b	00		.
	nop			;7c8c	00		.
	nop			;7c8d	00		.
	nop			;7c8e	00		.
	nop			;7c8f	00		.
	nop			;7c90	00		.
	nop			;7c91	00		.
	nop			;7c92	00		.
	nop			;7c93	00		.
	nop			;7c94	00		.
	nop			;7c95	00		.
	nop			;7c96	00		.
	nop			;7c97	00		.
	nop			;7c98	00		.
	nop			;7c99	00		.
	nop			;7c9a	00		.
	nop			;7c9b	00		.
	nop			;7c9c	00		.
	nop			;7c9d	00		.
	nop			;7c9e	00		.
	nop			;7c9f	00		.
	nop			;7ca0	00		.
	nop			;7ca1	00		.
	nop			;7ca2	00		.
	nop			;7ca3	00		.
	nop			;7ca4	00		.
	nop			;7ca5	00		.
	nop			;7ca6	00		.
	nop			;7ca7	00		.
	nop			;7ca8	00		.
	nop			;7ca9	00		.
	nop			;7caa	00		.
	nop			;7cab	00		.
	nop			;7cac	00		.
	nop			;7cad	00		.
	nop			;7cae	00		.
	nop			;7caf	00		.
	nop			;7cb0	00		.
	nop			;7cb1	00		.
	nop			;7cb2	00		.
	nop			;7cb3	00		.
	nop			;7cb4	00		.
	nop			;7cb5	00		.
	nop			;7cb6	00		.
	nop			;7cb7	00		.
	nop			;7cb8	00		.
	nop			;7cb9	00		.
	nop			;7cba	00		.
	nop			;7cbb	00		.
	nop			;7cbc	00		.
	nop			;7cbd	00		.
	nop			;7cbe	00		.
	nop			;7cbf	00		.
	nop			;7cc0	00		.
	nop			;7cc1	00		.
	nop			;7cc2	00		.
	nop			;7cc3	00		.
	nop			;7cc4	00		.
	nop			;7cc5	00		.
	nop			;7cc6	00		.
	nop			;7cc7	00		.
	nop			;7cc8	00		.
	nop			;7cc9	00		.
	nop			;7cca	00		.
	nop			;7ccb	00		.
	nop			;7ccc	00		.
	nop			;7ccd	00		.
	nop			;7cce	00		.
	nop			;7ccf	00		.
	nop			;7cd0	00		.
	nop			;7cd1	00		.
	nop			;7cd2	00		.
	nop			;7cd3	00		.
	nop			;7cd4	00		.
	nop			;7cd5	00		.
	nop			;7cd6	00		.
	nop			;7cd7	00		.
	nop			;7cd8	00		.
	nop			;7cd9	00		.
	nop			;7cda	00		.
	nop			;7cdb	00		.
	nop			;7cdc	00		.
	nop			;7cdd	00		.
	nop			;7cde	00		.
	nop			;7cdf	00		.
	nop			;7ce0	00		.
	nop			;7ce1	00		.
	nop			;7ce2	00		.
	nop			;7ce3	00		.
	nop			;7ce4	00		.
	nop			;7ce5	00		.
l7ce6h:
	nop			;7ce6	00		.
	nop			;7ce7	00		.
	nop			;7ce8	00		.
	nop			;7ce9	00		.
	nop			;7cea	00		.
	nop			;7ceb	00		.
	nop			;7cec	00		.
	nop			;7ced	00		.
	nop			;7cee	00		.
	nop			;7cef	00		.
	nop			;7cf0	00		.
	nop			;7cf1	00		.
	nop			;7cf2	00		.
	nop			;7cf3	00		.
	nop			;7cf4	00		.
	nop			;7cf5	00		.
	nop			;7cf6	00		.
	nop			;7cf7	00		.
sub_7cf8h:
	nop			;7cf8	00		.
	nop			;7cf9	00		.
	nop			;7cfa	00		.
	nop			;7cfb	00		.
	nop			;7cfc	00		.
	nop			;7cfd	00		.
	nop			;7cfe	00		.
	nop			;7cff	00		.
	nop			;7d00	00		.
	nop			;7d01	00		.
	nop			;7d02	00		.
	nop			;7d03	00		.
	nop			;7d04	00		.
	nop			;7d05	00		.
	nop			;7d06	00		.
	nop			;7d07	00		.
	nop			;7d08	00		.
	nop			;7d09	00		.
	nop			;7d0a	00		.
	nop			;7d0b	00		.
	nop			;7d0c	00		.
	nop			;7d0d	00		.
	nop			;7d0e	00		.
	nop			;7d0f	00		.
	nop			;7d10	00		.
	nop			;7d11	00		.
	nop			;7d12	00		.
	nop			;7d13	00		.
	nop			;7d14	00		.
	nop			;7d15	00		.
	nop			;7d16	00		.
	nop			;7d17	00		.
	nop			;7d18	00		.
	nop			;7d19	00		.
	nop			;7d1a	00		.
	nop			;7d1b	00		.
	nop			;7d1c	00		.
	nop			;7d1d	00		.
	nop			;7d1e	00		.
	nop			;7d1f	00		.
	nop			;7d20	00		.
	nop			;7d21	00		.
	nop			;7d22	00		.
	nop			;7d23	00		.
	nop			;7d24	00		.
	nop			;7d25	00		.
	nop			;7d26	00		.
	nop			;7d27	00		.
	nop			;7d28	00		.
	nop			;7d29	00		.
	nop			;7d2a	00		.
	nop			;7d2b	00		.
	nop			;7d2c	00		.
	nop			;7d2d	00		.
	nop			;7d2e	00		.
	nop			;7d2f	00		.
	nop			;7d30	00		.
	nop			;7d31	00		.
	nop			;7d32	00		.
	nop			;7d33	00		.
	nop			;7d34	00		.
	nop			;7d35	00		.
	nop			;7d36	00		.
	nop			;7d37	00		.
	nop			;7d38	00		.
	nop			;7d39	00		.
	nop			;7d3a	00		.
	nop			;7d3b	00		.
	nop			;7d3c	00		.
	nop			;7d3d	00		.
	nop			;7d3e	00		.
	nop			;7d3f	00		.
	nop			;7d40	00		.
	nop			;7d41	00		.
	nop			;7d42	00		.
	nop			;7d43	00		.
	nop			;7d44	00		.
	nop			;7d45	00		.
	nop			;7d46	00		.
	nop			;7d47	00		.
	nop			;7d48	00		.
	nop			;7d49	00		.
	nop			;7d4a	00		.
	nop			;7d4b	00		.
	nop			;7d4c	00		.
	nop			;7d4d	00		.
	nop			;7d4e	00		.
	nop			;7d4f	00		.
	nop			;7d50	00		.
	nop			;7d51	00		.
	nop			;7d52	00		.
	nop			;7d53	00		.
	nop			;7d54	00		.
	nop			;7d55	00		.
	nop			;7d56	00		.
	nop			;7d57	00		.
	nop			;7d58	00		.
	nop			;7d59	00		.
	nop			;7d5a	00		.
	nop			;7d5b	00		.
	nop			;7d5c	00		.
	nop			;7d5d	00		.
	nop			;7d5e	00		.
	nop			;7d5f	00		.
	nop			;7d60	00		.
	nop			;7d61	00		.
	nop			;7d62	00		.
	nop			;7d63	00		.
	nop			;7d64	00		.
	nop			;7d65	00		.
	nop			;7d66	00		.
	nop			;7d67	00		.
	nop			;7d68	00		.
	nop			;7d69	00		.
	nop			;7d6a	00		.
	nop			;7d6b	00		.
	nop			;7d6c	00		.
	nop			;7d6d	00		.
	nop			;7d6e	00		.
	nop			;7d6f	00		.
	nop			;7d70	00		.
	nop			;7d71	00		.
	nop			;7d72	00		.
	nop			;7d73	00		.
	nop			;7d74	00		.
	nop			;7d75	00		.
	nop			;7d76	00		.
	nop			;7d77	00		.
	nop			;7d78	00		.
	nop			;7d79	00		.
	nop			;7d7a	00		.
	nop			;7d7b	00		.
	nop			;7d7c	00		.
	nop			;7d7d	00		.
	nop			;7d7e	00		.
	nop			;7d7f	00		.
	nop			;7d80	00		.
	nop			;7d81	00		.
	nop			;7d82	00		.
	nop			;7d83	00		.
	nop			;7d84	00		.
	nop			;7d85	00		.
	nop			;7d86	00		.
	nop			;7d87	00		.
	nop			;7d88	00		.
	nop			;7d89	00		.
	nop			;7d8a	00		.
	nop			;7d8b	00		.
	nop			;7d8c	00		.
	nop			;7d8d	00		.
	nop			;7d8e	00		.
	nop			;7d8f	00		.
	nop			;7d90	00		.
	nop			;7d91	00		.
	nop			;7d92	00		.
	nop			;7d93	00		.
	nop			;7d94	00		.
	nop			;7d95	00		.
	nop			;7d96	00		.
	nop			;7d97	00		.
	nop			;7d98	00		.
	nop			;7d99	00		.
	nop			;7d9a	00		.
	nop			;7d9b	00		.
	nop			;7d9c	00		.
	nop			;7d9d	00		.
	nop			;7d9e	00		.
	nop			;7d9f	00		.
	nop			;7da0	00		.
	nop			;7da1	00		.
	nop			;7da2	00		.
	nop			;7da3	00		.
	nop			;7da4	00		.
	nop			;7da5	00		.
	nop			;7da6	00		.
	nop			;7da7	00		.
	nop			;7da8	00		.
	nop			;7da9	00		.
	nop			;7daa	00		.
	nop			;7dab	00		.
	nop			;7dac	00		.
	nop			;7dad	00		.
	nop			;7dae	00		.
	nop			;7daf	00		.
	nop			;7db0	00		.
	nop			;7db1	00		.
	nop			;7db2	00		.
	nop			;7db3	00		.
	nop			;7db4	00		.
	nop			;7db5	00		.
	nop			;7db6	00		.
	nop			;7db7	00		.
	nop			;7db8	00		.
	nop			;7db9	00		.
	nop			;7dba	00		.
	nop			;7dbb	00		.
	nop			;7dbc	00		.
	nop			;7dbd	00		.
	nop			;7dbe	00		.
	nop			;7dbf	00		.
	nop			;7dc0	00		.
	nop			;7dc1	00		.
	nop			;7dc2	00		.
	nop			;7dc3	00		.
	nop			;7dc4	00		.
	nop			;7dc5	00		.
	nop			;7dc6	00		.
	nop			;7dc7	00		.
	nop			;7dc8	00		.
	nop			;7dc9	00		.
	nop			;7dca	00		.
	nop			;7dcb	00		.
	nop			;7dcc	00		.
	nop			;7dcd	00		.
	nop			;7dce	00		.
	nop			;7dcf	00		.
	nop			;7dd0	00		.
	nop			;7dd1	00		.
	ld bc,06008h		;7dd2	01 08 60	. . `
	nop			;7dd5	00		.
	ret p			;7dd6	f0		.
	ld b,b			;7dd7	40		@
	ret p			;7dd8	f0		.
	ld b,b			;7dd9	40		@
	ret p			;7dda	f0		.
	ld b,b			;7ddb	40		@
	ld h,b			;7ddc	60		`
	nop			;7ddd	00		.
	ret p			;7dde	f0		.
	and b			;7ddf	a0		.
	ret p			;7de0	f0		.
	and b			;7de1	a0		.
	ret p			;7de2	f0		.
	and b			;7de3	a0		.
	ld bc,06008h		;7de4	01 08 60	. . `
	nop			;7de7	00		.
	ret p			;7de8	f0		.
	ld b,b			;7de9	40		@
	ret p			;7dea	f0		.
	ld b,b			;7deb	40		@
	ret p			;7dec	f0		.
	ld b,b			;7ded	40		@
	ld h,b			;7dee	60		`
	nop			;7def	00		.
	ret p			;7df0	f0		.
	ld d,b			;7df1	50		P
	ret p			;7df2	f0		.
	ld d,b			;7df3	50		P
	ret p			;7df4	f0		.
	ld d,b			;7df5	50		P
	ld bc,00006h		;7df6	01 06 00	. . .
	nop			;7df9	00		.
	nop			;7dfa	00		.
	nop			;7dfb	00		.
	jr l7dfeh		;7dfc	18 00		. .
l7dfeh:
	inc a			;7dfe	3c		<
	jr l7e3dh		;7dff	18 3c		. <
	jr l7e1bh		;7e01	18 18		. .
	nop			;7e03	00		.
	ld bc,00007h		;7e04	01 07 00	. . .
	nop			;7e07	00		.
	jr l7e0ah		;7e08	18 00		. .
l7e0ah:
	inc a			;7e0a	3c		<
	jr l7e8bh		;7e0b	18 7e		. ~
	inc h			;7e0d	24		$
	ld a,(hl)		;7e0e	7e		~
	inc h			;7e0f	24		$
	inc a			;7e10	3c		<
	jr $+26			;7e11	18 18		. .
	nop			;7e13	00		.
	ld bc,02408h		;7e14	01 08 24	. . $
	nop			;7e17	00		.
	ld a,(hl)		;7e18	7e		~
	inc h			;7e19	24		$
	rst 20h			;7e1a	e7		.
l7e1bh:
	ld b,d			;7e1b	42		B
	jp po,04740h		;7e1c	e2 40 47	. @ G
	ld (bc),a		;7e1f	02		.
	rst 20h			;7e20	e7		.
	ld b,d			;7e21	42		B
	ld a,(hl)		;7e22	7e		~
	inc h			;7e23	24		$
	inc h			;7e24	24		$
	nop			;7e25	00		.
	ld bc,06608h		;7e26	01 08 66	. . f
	nop			;7e29	00		.
	jp 08900h		;7e2a	c3 00 89	. . .
	nop			;7e2d	00		.
	jr nz,l7e30h		;7e2e	20 00		  .
l7e30h:
	nop			;7e30	00		.
	nop			;7e31	00		.
	adc a,c			;7e32	89		.
	nop			;7e33	00		.
	jp 06600h		;7e34	c3 00 66	. . f
	nop			;7e37	00		.
	inc b			;7e38	04		.
	dec c			;7e39	0d		.
	ccf			;7e3a	3f		?
	nop			;7e3b	00		.
	rst 38h			;7e3c	ff		.
l7e3dh:
	nop			;7e3d	00		.
	rst 38h			;7e3e	ff		.
	nop			;7e3f	00		.
	ret nz			;7e40	c0		.
	nop			;7e41	00		.
	ld a,a			;7e42	7f		.
	dec de			;7e43	1b		.
	rst 38h			;7e44	ff		.
	ld a,a			;7e45	7f		.
	rst 38h			;7e46	ff		.
	defb 0edh ;next byte illegal after ed	;7e47	ed		.
	ret po			;7e48	e0		.
	add a,b			;7e49	80		.
	rst 38h			;7e4a	ff		.
	inc (hl)		;7e4b	34		4
	rst 38h			;7e4c	ff		.
	add a,b			;7e4d	80		.
	rst 38h			;7e4e	ff		.
	ld (de),a		;7e4f	12		.
	ret p			;7e50	f0		.
	ret nz			;7e51	c0		.
	rst 38h			;7e52	ff		.
	ld h,h			;7e53	64		d
	rst 38h			;7e54	ff		.
	add a,b			;7e55	80		.
	rst 38h			;7e56	ff		.
	ld (de),a		;7e57	12		.
	ret p			;7e58	f0		.
	ld h,b			;7e59	60		`
	rst 38h			;7e5a	ff		.
	ld c,e			;7e5b	4b		K
	rst 38h			;7e5c	ff		.
	ld a,a			;7e5d	7f		.
	rst 38h			;7e5e	ff		.
	defb 0edh ;next byte illegal after ed	;7e5f	ed		.
	ret m			;7e60	f8		.
	jr nz,$+1		;7e61	20 ff		  .
	ld a,e			;7e63	7b		{
	rst 38h			;7e64	ff		.
	ld a,a			;7e65	7f		.
	rst 38h			;7e66	ff		.
	defb 0edh ;next byte illegal after ed	;7e67	ed		.
	call p,0ffe0h		;7e68	f4 e0 ff	. . .
	ld a,e			;7e6b	7b		{
	rst 38h			;7e6c	ff		.
	ld a,a			;7e6d	7f		.
	rst 38h			;7e6e	ff		.
	defb 0edh ;next byte illegal after ed	;7e6f	ed		.
	jp m,0ffe0h		;7e70	fa e0 ff	. . .
	dec sp			;7e73	3b		;
	rst 38h			;7e74	ff		.
	ld a,a			;7e75	7f		.
	rst 38h			;7e76	ff		.
	defb 0edh ;next byte illegal after ed	;7e77	ed		.
	push af			;7e78	f5		.
	ret nz			;7e79	c0		.
	ld a,a			;7e7a	7f		.
	dec de			;7e7b	1b		.
	rst 38h			;7e7c	ff		.
	ld a,a			;7e7d	7f		.
	rst 38h			;7e7e	ff		.
	defb 0edh ;next byte illegal after ed	;7e7f	ed		.
	jp pe,03f80h		;7e80	ea 80 3f	. . ?
	nop			;7e83	00		.
	rst 38h			;7e84	ff		.
	nop			;7e85	00		.
	rst 38h			;7e86	ff		.
	nop			;7e87	00		.
	push de			;7e88	d5		.
	nop			;7e89	00		.
	ld a,(bc)		;7e8a	0a		.
l7e8bh:
	nop			;7e8b	00		.
	xor d			;7e8c	aa		.
	nop			;7e8d	00		.
	xor d			;7e8e	aa		.
	nop			;7e8f	00		.
	xor d			;7e90	aa		.
	nop			;7e91	00		.
	dec b			;7e92	05		.
	nop			;7e93	00		.
	ld d,l			;7e94	55		U
	nop			;7e95	00		.
	ld d,l			;7e96	55		U
	nop			;7e97	00		.
	ld d,h			;7e98	54		T
	nop			;7e99	00		.
	ld (bc),a		;7e9a	02		.
	nop			;7e9b	00		.
	xor d			;7e9c	aa		.
	nop			;7e9d	00		.
	xor d			;7e9e	aa		.
	nop			;7e9f	00		.
	xor b			;7ea0	a8		.
	nop			;7ea1	00		.
	inc bc			;7ea2	03		.
	dec c			;7ea3	0d		.
	rst 38h			;7ea4	ff		.
	nop			;7ea5	00		.
	rst 38h			;7ea6	ff		.
	nop			;7ea7	00		.
	ret nz			;7ea8	c0		.
	nop			;7ea9	00		.
	ld a,a			;7eaa	7f		.
	ld a,a			;7eab	7f		.
	rst 38h			;7eac	ff		.
	defb 0edh ;next byte illegal after ed	;7ead	ed		.
	ret po			;7eae	e0		.
	add a,b			;7eaf	80		.
	ld a,a			;7eb0	7f		.
	nop			;7eb1	00		.
	rst 38h			;7eb2	ff		.
	ld (de),a		;7eb3	12		.
	ret p			;7eb4	f0		.
	ret nz			;7eb5	c0		.
	ld a,a			;7eb6	7f		.
	nop			;7eb7	00		.
	rst 38h			;7eb8	ff		.
	ld (de),a		;7eb9	12		.
	ret p			;7eba	f0		.
	ld h,b			;7ebb	60		`
	ld a,a			;7ebc	7f		.
	ld a,a			;7ebd	7f		.
	rst 38h			;7ebe	ff		.
	defb 0edh ;next byte illegal after ed	;7ebf	ed		.
	ret m			;7ec0	f8		.
	jr nz,l7f42h		;7ec1	20 7f		  .
	ld a,a			;7ec3	7f		.
	rst 38h			;7ec4	ff		.
	defb 0edh ;next byte illegal after ed	;7ec5	ed		.
sub_7ec6h:
	call p,sub_7fe0h	;7ec6	f4 e0 7f	. . .
	ld a,a			;7ec9	7f		.
	rst 38h			;7eca	ff		.
	defb 0edh ;next byte illegal after ed	;7ecb	ed		.
	jp m,sub_7fe0h		;7ecc	fa e0 7f	. . .
	ld a,a			;7ecf	7f		.
	rst 38h			;7ed0	ff		.
	defb 0edh ;next byte illegal after ed	;7ed1	ed		.
	push af			;7ed2	f5		.
	ret nz			;7ed3	c0		.
	ld a,a			;7ed4	7f		.
	ld a,a			;7ed5	7f		.
	rst 38h			;7ed6	ff		.
	defb 0edh ;next byte illegal after ed	;7ed7	ed		.
	jp pe,0ff80h		;7ed8	ea 80 ff	. . .
	nop			;7edb	00		.
	rst 38h			;7edc	ff		.
	nop			;7edd	00		.
	push de			;7ede	d5		.
	nop			;7edf	00		.
	xor d			;7ee0	aa		.
	nop			;7ee1	00		.
	xor d			;7ee2	aa		.
	nop			;7ee3	00		.
	xor d			;7ee4	aa		.
	nop			;7ee5	00		.
	ld d,l			;7ee6	55		U
	nop			;7ee7	00		.
	ld d,l			;7ee8	55		U
	nop			;7ee9	00		.
	ld d,h			;7eea	54		T
	nop			;7eeb	00		.
	xor d			;7eec	aa		.
	nop			;7eed	00		.
	xor d			;7eee	aa		.
	nop			;7eef	00		.
	xor b			;7ef0	a8		.
	nop			;7ef1	00		.
	inc bc			;7ef2	03		.
	dec c			;7ef3	0d		.
	ccf			;7ef4	3f		?
	nop			;7ef5	00		.
	rst 38h			;7ef6	ff		.
	nop			;7ef7	00		.
	rst 38h			;7ef8	ff		.
	nop			;7ef9	00		.
	ld a,a			;7efa	7f		.
	dec de			;7efb	1b		.
	rst 38h			;7efc	ff		.
	ld a,a			;7efd	7f		.
	rst 38h			;7efe	ff		.
l7effh:
	rst 38h			;7eff	ff		.
	rst 38h			;7f00	ff		.
	inc (hl)		;7f01	34		4
	rst 38h			;7f02	ff		.
	add a,b			;7f03	80		.
	rst 38h			;7f04	ff		.
	nop			;7f05	00		.
	rst 38h			;7f06	ff		.
	ld h,h			;7f07	64		d
	rst 38h			;7f08	ff		.
	add a,b			;7f09	80		.
	rst 38h			;7f0a	ff		.
	nop			;7f0b	00		.
	rst 38h			;7f0c	ff		.
	ld c,e			;7f0d	4b		K
	rst 38h			;7f0e	ff		.
	ld a,a			;7f0f	7f		.
	rst 38h			;7f10	ff		.
	rst 38h			;7f11	ff		.
	rst 38h			;7f12	ff		.
	ld a,e			;7f13	7b		{
	rst 38h			;7f14	ff		.
	ld a,a			;7f15	7f		.
	rst 38h			;7f16	ff		.
	rst 38h			;7f17	ff		.
	rst 38h			;7f18	ff		.
	ld a,e			;7f19	7b		{
	rst 38h			;7f1a	ff		.
	ld a,a			;7f1b	7f		.
	rst 38h			;7f1c	ff		.
	rst 38h			;7f1d	ff		.
	rst 38h			;7f1e	ff		.
	dec sp			;7f1f	3b		;
	rst 38h			;7f20	ff		.
	ld a,a			;7f21	7f		.
	rst 38h			;7f22	ff		.
	rst 38h			;7f23	ff		.
	ld a,a			;7f24	7f		.
	dec de			;7f25	1b		.
	rst 38h			;7f26	ff		.
	ld a,a			;7f27	7f		.
	rst 38h			;7f28	ff		.
	rst 38h			;7f29	ff		.
	ccf			;7f2a	3f		?
	nop			;7f2b	00		.
	rst 38h			;7f2c	ff		.
	nop			;7f2d	00		.
	rst 38h			;7f2e	ff		.
	nop			;7f2f	00		.
	ld a,(bc)		;7f30	0a		.
	nop			;7f31	00		.
	xor d			;7f32	aa		.
	nop			;7f33	00		.
	xor d			;7f34	aa		.
	nop			;7f35	00		.
	dec b			;7f36	05		.
	nop			;7f37	00		.
	ld d,l			;7f38	55		U
	nop			;7f39	00		.
	ld d,l			;7f3a	55		U
	nop			;7f3b	00		.
	ld (bc),a		;7f3c	02		.
	nop			;7f3d	00		.
	xor d			;7f3e	aa		.
	nop			;7f3f	00		.
	xor d			;7f40	aa		.
	nop			;7f41	00		.
l7f42h:
	ld b,00dh		;7f42	06 0d		. .
	ccf			;7f44	3f		?
	nop			;7f45	00		.
	rst 38h			;7f46	ff		.
	nop			;7f47	00		.
sub_7f48h:
	rst 38h			;7f48	ff		.
	nop			;7f49	00		.
	rst 38h			;7f4a	ff		.
	nop			;7f4b	00		.
	rst 38h			;7f4c	ff		.
	nop			;7f4d	00		.
	ret nz			;7f4e	c0		.
	nop			;7f4f	00		.
	ld a,a			;7f50	7f		.
	dec de			;7f51	1b		.
	rst 38h			;7f52	ff		.
	ld a,a			;7f53	7f		.
	rst 38h			;7f54	ff		.
	rst 38h			;7f55	ff		.
	rst 38h			;7f56	ff		.
	rst 38h			;7f57	ff		.
	rst 38h			;7f58	ff		.
	defb 0edh ;next byte illegal after ed	;7f59	ed		.
	ret po			;7f5a	e0		.
	add a,b			;7f5b	80		.
	rst 38h			;7f5c	ff		.
	inc (hl)		;7f5d	34		4
	rst 38h			;7f5e	ff		.
	add a,b			;7f5f	80		.
	rst 38h			;7f60	ff		.
	nop			;7f61	00		.
	rst 38h			;7f62	ff		.
	nop			;7f63	00		.
	rst 38h			;7f64	ff		.
	ld (de),a		;7f65	12		.
	ret p			;7f66	f0		.
	ret nz			;7f67	c0		.
	rst 38h			;7f68	ff		.
	ld h,h			;7f69	64		d
	rst 38h			;7f6a	ff		.
	add a,b			;7f6b	80		.
	rst 38h			;7f6c	ff		.
	nop			;7f6d	00		.
	rst 38h			;7f6e	ff		.
	nop			;7f6f	00		.
	rst 38h			;7f70	ff		.
	ld (de),a		;7f71	12		.
	ret p			;7f72	f0		.
	ld h,b			;7f73	60		`
	rst 38h			;7f74	ff		.
	ld c,e			;7f75	4b		K
	rst 38h			;7f76	ff		.
	ld a,a			;7f77	7f		.
sub_7f78h:
	rst 38h			;7f78	ff		.
	rst 38h			;7f79	ff		.
	rst 38h			;7f7a	ff		.
	rst 38h			;7f7b	ff		.
	rst 38h			;7f7c	ff		.
	defb 0edh ;next byte illegal after ed	;7f7d	ed		.
	ret m			;7f7e	f8		.
	jr nz,$+1		;7f7f	20 ff		  .
	ld a,e			;7f81	7b		{
	rst 38h			;7f82	ff		.
	ld a,a			;7f83	7f		.
	rst 38h			;7f84	ff		.
	rst 38h			;7f85	ff		.
	rst 38h			;7f86	ff		.
	rst 38h			;7f87	ff		.
	rst 38h			;7f88	ff		.
	defb 0edh ;next byte illegal after ed	;7f89	ed		.
	call p,0ffe0h		;7f8a	f4 e0 ff	. . .
	ld a,e			;7f8d	7b		{
	rst 38h			;7f8e	ff		.
	ld a,a			;7f8f	7f		.
	rst 38h			;7f90	ff		.
	rst 38h			;7f91	ff		.
	rst 38h			;7f92	ff		.
	rst 38h			;7f93	ff		.
	rst 38h			;7f94	ff		.
	defb 0edh ;next byte illegal after ed	;7f95	ed		.
	jp m,0ffe0h		;7f96	fa e0 ff	. . .
	dec sp			;7f99	3b		;
	rst 38h			;7f9a	ff		.
	ld a,a			;7f9b	7f		.
	rst 38h			;7f9c	ff		.
	rst 38h			;7f9d	ff		.
	rst 38h			;7f9e	ff		.
	rst 38h			;7f9f	ff		.
	rst 38h			;7fa0	ff		.
	defb 0edh ;next byte illegal after ed	;7fa1	ed		.
	push af			;7fa2	f5		.
	ret nz			;7fa3	c0		.
	ld a,a			;7fa4	7f		.
	dec de			;7fa5	1b		.
	rst 38h			;7fa6	ff		.
	ld a,a			;7fa7	7f		.
	rst 38h			;7fa8	ff		.
	rst 38h			;7fa9	ff		.
	rst 38h			;7faa	ff		.
	rst 38h			;7fab	ff		.
	rst 38h			;7fac	ff		.
	defb 0edh ;next byte illegal after ed	;7fad	ed		.
	jp pe,03f80h		;7fae	ea 80 3f	. . ?
	nop			;7fb1	00		.
	rst 38h			;7fb2	ff		.
	nop			;7fb3	00		.
	rst 38h			;7fb4	ff		.
	nop			;7fb5	00		.
	rst 38h			;7fb6	ff		.
	nop			;7fb7	00		.
	rst 38h			;7fb8	ff		.
	nop			;7fb9	00		.
	push de			;7fba	d5		.
	nop			;7fbb	00		.
	ld a,(bc)		;7fbc	0a		.
	nop			;7fbd	00		.
	xor d			;7fbe	aa		.
	nop			;7fbf	00		.
sub_7fc0h:
	xor d			;7fc0	aa		.
	nop			;7fc1	00		.
	xor d			;7fc2	aa		.
	nop			;7fc3	00		.
	xor d			;7fc4	aa		.
	nop			;7fc5	00		.
	xor d			;7fc6	aa		.
	nop			;7fc7	00		.
	dec b			;7fc8	05		.
	nop			;7fc9	00		.
	ld d,l			;7fca	55		U
	nop			;7fcb	00		.
	ld d,l			;7fcc	55		U
	nop			;7fcd	00		.
	ld d,l			;7fce	55		U
	nop			;7fcf	00		.
	ld d,l			;7fd0	55		U
	nop			;7fd1	00		.
	ld d,h			;7fd2	54		T
	nop			;7fd3	00		.
	ld (bc),a		;7fd4	02		.
	nop			;7fd5	00		.
	xor d			;7fd6	aa		.
	nop			;7fd7	00		.
sub_7fd8h:
	xor d			;7fd8	aa		.
	nop			;7fd9	00		.
	xor d			;7fda	aa		.
	nop			;7fdb	00		.
	xor d			;7fdc	aa		.
	nop			;7fdd	00		.
	xor b			;7fde	a8		.
	nop			;7fdf	00		.
sub_7fe0h:
	inc b			;7fe0	04		.
	dec c			;7fe1	0d		.
	rra			;7fe2	1f		.
	nop			;7fe3	00		.
	rst 38h			;7fe4	ff		.
	nop			;7fe5	00		.
	rst 38h			;7fe6	ff		.
	nop			;7fe7	00		.
	add a,b			;7fe8	80		.
	nop			;7fe9	00		.
	ccf			;7fea	3f		?
	dec c			;7feb	0d		.
	rst 38h			;7fec	ff		.
	rst 38h			;7fed	ff		.
	rst 38h			;7fee	ff		.
	ei			;7fef	fb		.
sub_7ff0h:
	ret nz			;7ff0	c0		.
	nop			;7ff1	00		.
	ld a,a			;7ff2	7f		.
	ld a,(de)		;7ff3	1a		.
	rst 38h			;7ff4	ff		.
	nop			;7ff5	00		.
	rst 38h			;7ff6	ff		.
	dec b			;7ff7	05		.
sub_7ff8h:
	ret po			;7ff8	e0		.
	add a,b			;7ff9	80		.
	ld a,a			;7ffa	7f		.
	ld (000ffh),a		;7ffb	32 ff 00	2 . .
	rst 38h			;7ffe	ff		.
l7fffh:
	inc b			;7fff	04		.
l8000h:
	ret p			;8000	f0		.
	ret nz			;8001	c0		.
l8002h:
	ld a,a			;8002	7f		.
	dec h			;8003	25		%
	rst 38h			;8004	ff		.
	rst 38h			;8005	ff		.
	rst 38h			;8006	ff		.
	jp m,040e8h		;8007	fa e8 40	. . @
	ld a,a			;800a	7f		.
	dec a			;800b	3d		=
	rst 38h			;800c	ff		.
	rst 38h			;800d	ff		.
	rst 38h			;800e	ff		.
	ei			;800f	fb		.
	call p,sub_7fc0h	;8010	f4 c0 7f	. . .
	dec a			;8013	3d		=
	rst 38h			;8014	ff		.
	rst 38h			;8015	ff		.
	rst 38h			;8016	ff		.
	ei			;8017	fb		.
	jp m,sub_7fc0h		;8018	fa c0 7f	. . .
	dec e			;801b	1d		.
	rst 38h			;801c	ff		.
	rst 38h			;801d	ff		.
	rst 38h			;801e	ff		.
	ei			;801f	fb		.
	call p,03f80h		;8020	f4 80 3f	. . ?
	dec c			;8023	0d		.
	rst 38h			;8024	ff		.
	rst 38h			;8025	ff		.
	rst 38h			;8026	ff		.
	ei			;8027	fb		.
	jp pe,01f00h		;8028	ea 00 1f	. . .
	nop			;802b	00		.
	rst 38h			;802c	ff		.
	nop			;802d	00		.
	rst 38h			;802e	ff		.
	nop			;802f	00		.
	call nc,00200h		;8030	d4 00 02	. . .
	nop			;8033	00		.
	xor d			;8034	aa		.
	nop			;8035	00		.
	xor d			;8036	aa		.
	nop			;8037	00		.
	xor d			;8038	aa		.
	nop			;8039	00		.
	ld bc,05500h		;803a	01 00 55	. . U
	nop			;803d	00		.
	ld d,l			;803e	55		U
	nop			;803f	00		.
	ld d,h			;8040	54		T
	nop			;8041	00		.
	nop			;8042	00		.
	nop			;8043	00		.
	xor d			;8044	aa		.
	nop			;8045	00		.
	xor d			;8046	aa		.
	nop			;8047	00		.
	xor b			;8048	a8		.
	nop			;8049	00		.
	inc b			;804a	04		.
	dec c			;804b	0d		.
	rra			;804c	1f		.
	nop			;804d	00		.
	rst 38h			;804e	ff		.
	nop			;804f	00		.
	rst 38h			;8050	ff		.
	nop			;8051	00		.
	add a,b			;8052	80		.
	nop			;8053	00		.
	ccf			;8054	3f		?
	dec c			;8055	0d		.
	rst 38h			;8056	ff		.
	rst 38h			;8057	ff		.
	rst 38h			;8058	ff		.
	ei			;8059	fb		.
	ret nz			;805a	c0		.
	nop			;805b	00		.
	ld a,a			;805c	7f		.
	ld a,(de)		;805d	1a		.
	rst 38h			;805e	ff		.
	nop			;805f	00		.
	rst 38h			;8060	ff		.
	dec b			;8061	05		.
	ret po			;8062	e0		.
	add a,b			;8063	80		.
	ld a,a			;8064	7f		.
	ld (00bffh),a		;8065	32 ff 0b	2 . .
	rst 38h			;8068	ff		.
	inc b			;8069	04		.
	ret p			;806a	f0		.
	ret nz			;806b	c0		.
	ld a,a			;806c	7f		.
	dec h			;806d	25		%
	rst 38h			;806e	ff		.
	ret p			;806f	f0		.
	rst 38h			;8070	ff		.
	jp m,040e8h		;8071	fa e8 40	. . @
	ld a,a			;8074	7f		.
	dec a			;8075	3d		=
	rst 38h			;8076	ff		.
	rst 38h			;8077	ff		.
	rst 38h			;8078	ff		.
	ei			;8079	fb		.
	call p,sub_7fc0h	;807a	f4 c0 7f	. . .
	ld a,0ffh		;807d	3e ff		> .
	rst 38h			;807f	ff		.
	rst 38h			;8080	ff		.
	rst 30h			;8081	f7		.
	jp m,sub_7fc0h		;8082	fa c0 7f	. . .
	ld e,0ffh		;8085	1e ff		. .
	rst 38h			;8087	ff		.
	rst 38h			;8088	ff		.
	rst 30h			;8089	f7		.
	call p,03f40h		;808a	f4 40 3f	. @ ?
	ld c,0ffh		;808d	0e ff		. .
	rst 38h			;808f	ff		.
	rst 38h			;8090	ff		.
	rst 30h			;8091	f7		.
	jp pe,01f00h		;8092	ea 00 1f	. . .
	nop			;8095	00		.
	rst 38h			;8096	ff		.
	nop			;8097	00		.
	rst 38h			;8098	ff		.
	nop			;8099	00		.
	call nc,00200h		;809a	d4 00 02	. . .
	nop			;809d	00		.
	xor d			;809e	aa		.
	nop			;809f	00		.
	xor d			;80a0	aa		.
	nop			;80a1	00		.
	xor d			;80a2	aa		.
	nop			;80a3	00		.
	ld bc,05500h		;80a4	01 00 55	. . U
	nop			;80a7	00		.
	ld d,l			;80a8	55		U
	nop			;80a9	00		.
	ld d,h			;80aa	54		T
	nop			;80ab	00		.
	nop			;80ac	00		.
	nop			;80ad	00		.
	xor d			;80ae	aa		.
	nop			;80af	00		.
	xor d			;80b0	aa		.
	nop			;80b1	00		.
	xor b			;80b2	a8		.
	nop			;80b3	00		.
	inc b			;80b4	04		.
	dec c			;80b5	0d		.
	ccf			;80b6	3f		?
	nop			;80b7	00		.
	rst 38h			;80b8	ff		.
	nop			;80b9	00		.
	rst 38h			;80ba	ff		.
	nop			;80bb	00		.
	ret nz			;80bc	c0		.
	nop			;80bd	00		.
	ld a,a			;80be	7f		.
	dec de			;80bf	1b		.
l80c0h:
	rst 38h			;80c0	ff		.
	ret p			;80c1	f0		.
	rst 38h			;80c2	ff		.
	defb 0fdh,0e0h,080h ;illegal sequence	;80c3	fd e0 80	. . .
	rst 38h			;80c6	ff		.
	inc (hl)		;80c7	34		4
	rst 38h			;80c8	ff		.
	dec bc			;80c9	0b		.
	rst 38h			;80ca	ff		.
	ld (bc),a		;80cb	02		.
	ret p			;80cc	f0		.
	ret nz			;80cd	c0		.
	rst 38h			;80ce	ff		.
	ld h,h			;80cf	64		d
	rst 38h			;80d0	ff		.
	inc de			;80d1	13		.
	rst 38h			;80d2	ff		.
	add a,d			;80d3	82		.
	ret p			;80d4	f0		.
	ld h,b			;80d5	60		`
	rst 38h			;80d6	ff		.
	ld c,e			;80d7	4b		K
	rst 38h			;80d8	ff		.
	ret po			;80d9	e0		.
	rst 38h			;80da	ff		.
	ld a,l			;80db	7d		}
	ret m			;80dc	f8		.
	jr nz,$+1		;80dd	20 ff		  .
	ld a,e			;80df	7b		{
	rst 38h			;80e0	ff		.
	rst 38h			;80e1	ff		.
	rst 38h			;80e2	ff		.
	defb 0fdh,0f4h,0e0h ;illegal sequence	;80e3	fd f4 e0	. . .
	rst 38h			;80e6	ff		.
	ld a,l			;80e7	7d		}
	rst 38h			;80e8	ff		.
	rst 38h			;80e9	ff		.
	rst 38h			;80ea	ff		.
	ei			;80eb	fb		.
	jp m,0ffe0h		;80ec	fa e0 ff	. . .
	dec a			;80ef	3d		=
	rst 38h			;80f0	ff		.
	rst 38h			;80f1	ff		.
	rst 38h			;80f2	ff		.
	ei			;80f3	fb		.
	push af			;80f4	f5		.
	ret nz			;80f5	c0		.
	ld a,a			;80f6	7f		.
	ld e,0ffh		;80f7	1e ff		. .
	rst 38h			;80f9	ff		.
	rst 38h			;80fa	ff		.
	rst 30h			;80fb	f7		.
	jp pe,03f80h		;80fc	ea 80 3f	. . ?
l80ffh:
	nop			;80ff	00		.
	rst 38h			;8100	ff		.
	nop			;8101	00		.
	rst 38h			;8102	ff		.
	nop			;8103	00		.
	push de			;8104	d5		.
	nop			;8105	00		.
	ld a,(bc)		;8106	0a		.
	nop			;8107	00		.
	xor d			;8108	aa		.
	nop			;8109	00		.
	xor d			;810a	aa		.
	nop			;810b	00		.
	xor d			;810c	aa		.
	nop			;810d	00		.
	dec b			;810e	05		.
	nop			;810f	00		.
	ld d,l			;8110	55		U
	nop			;8111	00		.
	ld d,l			;8112	55		U
	nop			;8113	00		.
	ld d,h			;8114	54		T
	nop			;8115	00		.
	ld (bc),a		;8116	02		.
	nop			;8117	00		.
	xor d			;8118	aa		.
	nop			;8119	00		.
	xor d			;811a	aa		.
	nop			;811b	00		.
	xor b			;811c	a8		.
	nop			;811d	00		.
	inc b			;811e	04		.
	dec c			;811f	0d		.
	ccf			;8120	3f		?
	nop			;8121	00		.
	rst 38h			;8122	ff		.
	nop			;8123	00		.
	rst 38h			;8124	ff		.
	nop			;8125	00		.
	ret nz			;8126	c0		.
	nop			;8127	00		.
	ld a,a			;8128	7f		.
	dec de			;8129	1b		.
	rst 38h			;812a	ff		.
	ex de,hl		;812b	eb		.
	rst 38h			;812c	ff		.
	ld a,l			;812d	7d		}
	ret po			;812e	e0		.
	add a,b			;812f	80		.
	rst 38h			;8130	ff		.
	inc (hl)		;8131	34		4
	rst 38h			;8132	ff		.
	inc de			;8133	13		.
	rst 38h			;8134	ff		.
	add a,d			;8135	82		.
	ret p			;8136	f0		.
	ret nz			;8137	c0		.
	rst 38h			;8138	ff		.
	ld h,h			;8139	64		d
	rst 38h			;813a	ff		.
	daa			;813b	27		'
	rst 38h			;813c	ff		.
	jp nz,060f0h		;813d	c2 f0 60	. . `
	rst 38h			;8140	ff		.
	ld c,e			;8141	4b		K
	rst 38h			;8142	ff		.
	ret nz			;8143	c0		.
	rst 38h			;8144	ff		.
	dec a			;8145	3d		=
	ret m			;8146	f8		.
	jr nz,$+1		;8147	20 ff		  .
	ld a,e			;8149	7b		{
	rst 38h			;814a	ff		.
	rst 38h			;814b	ff		.
	rst 38h			;814c	ff		.
	defb 0fdh,0f4h,0e0h ;illegal sequence	;814d	fd f4 e0	. . .
	rst 38h			;8150	ff		.
	ld a,l			;8151	7d		}
	rst 38h			;8152	ff		.
	rst 38h			;8153	ff		.
	rst 38h			;8154	ff		.
	ei			;8155	fb		.
	jp m,0ffe0h		;8156	fa e0 ff	. . .
	ld a,0ffh		;8159	3e ff		> .
	rst 38h			;815b	ff		.
	rst 38h			;815c	ff		.
	rst 30h			;815d	f7		.
	push af			;815e	f5		.
	ret nz			;815f	c0		.
	ld a,a			;8160	7f		.
	rra			;8161	1f		.
	rst 38h			;8162	ff		.
	ld a,a			;8163	7f		.
	rst 38h			;8164	ff		.
	rst 28h			;8165	ef		.
	jp pe,03f80h		;8166	ea 80 3f	. . ?
	nop			;8169	00		.
	rst 38h			;816a	ff		.
	nop			;816b	00		.
	rst 38h			;816c	ff		.
	nop			;816d	00		.
	push de			;816e	d5		.
	nop			;816f	00		.
	ld a,(bc)		;8170	0a		.
	nop			;8171	00		.
	xor d			;8172	aa		.
	nop			;8173	00		.
	xor d			;8174	aa		.
	nop			;8175	00		.
	xor d			;8176	aa		.
	nop			;8177	00		.
	dec b			;8178	05		.
	nop			;8179	00		.
	ld d,l			;817a	55		U
	nop			;817b	00		.
	ld d,l			;817c	55		U
	nop			;817d	00		.
	ld d,h			;817e	54		T
	nop			;817f	00		.
	ld (bc),a		;8180	02		.
	nop			;8181	00		.
	xor d			;8182	aa		.
	nop			;8183	00		.
	xor d			;8184	aa		.
	nop			;8185	00		.
	xor b			;8186	a8		.
	nop			;8187	00		.
	inc b			;8188	04		.
	dec c			;8189	0d		.
	nop			;818a	00		.
	nop			;818b	00		.
	rra			;818c	1f		.
	nop			;818d	00		.
	add a,b			;818e	80		.
	nop			;818f	00		.
	nop			;8190	00		.
	nop			;8191	00		.
	ld a,a			;8192	7f		.
	nop			;8193	00		.
	rst 38h			;8194	ff		.
	dec bc			;8195	0b		.
	rst 38h			;8196	ff		.
	nop			;8197	00		.
	ret po			;8198	e0		.
	nop			;8199	00		.
	rst 38h			;819a	ff		.
	dec sp			;819b	3b		;
	rst 38h			;819c	ff		.
	out (0ffh),a		;819d	d3 ff		. .
	cp l			;819f	bd		.
	ret p			;81a0	f0		.
	ret nz			;81a1	c0		.
	rst 38h			;81a2	ff		.
	ld h,h			;81a3	64		d
	rst 38h			;81a4	ff		.
	daa			;81a5	27		'
	rst 38h			;81a6	ff		.
	jp nz,060f0h		;81a7	c2 f0 60	. . `
	rst 38h			;81aa	ff		.
	ld b,h			;81ab	44		D
	rst 38h			;81ac	ff		.
	rrca			;81ad	0f		.
	rst 38h			;81ae	ff		.
	ld (bc),a		;81af	02		.
	ret m			;81b0	f8		.
	jr nz,$+1		;81b1	20 ff		  .
	ld a,e			;81b3	7b		{
	rst 38h			;81b4	ff		.
	ret p			;81b5	f0		.
	rst 38h			;81b6	ff		.
	defb 0fdh,0f4h,0e0h ;illegal sequence	;81b7	fd f4 e0	. . .
	rst 38h			;81ba	ff		.
	ld a,l			;81bb	7d		}
	rst 38h			;81bc	ff		.
	rst 38h			;81bd	ff		.
	rst 38h			;81be	ff		.
	ei			;81bf	fb		.
	jp m,0ffe0h		;81c0	fa e0 ff	. . .
	ld a,0ffh		;81c3	3e ff		> .
	rst 38h			;81c5	ff		.
	rst 38h			;81c6	ff		.
	rst 30h			;81c7	f7		.
	push af			;81c8	f5		.
	ret nz			;81c9	c0		.
	ld a,a			;81ca	7f		.
	rrca			;81cb	0f		.
	rst 38h			;81cc	ff		.
	ld a,a			;81cd	7f		.
	rst 38h			;81ce	ff		.
	rst 28h			;81cf	ef		.
	jp pe,01f00h		;81d0	ea 00 1f	. . .
	nop			;81d3	00		.
	rst 38h			;81d4	ff		.
	ccf			;81d5	3f		?
	rst 38h			;81d6	ff		.
	ret nz			;81d7	c0		.
	push de			;81d8	d5		.
	nop			;81d9	00		.
	ld a,(bc)		;81da	0a		.
	nop			;81db	00		.
	rst 38h			;81dc	ff		.
	nop			;81dd	00		.
	jp pe,la9ffh+1		;81de	ea 00 aa	. . .
	nop			;81e1	00		.
	dec b			;81e2	05		.
	nop			;81e3	00		.
	ld d,l			;81e4	55		U
	nop			;81e5	00		.
	ld d,l			;81e6	55		U
	nop			;81e7	00		.
	ld d,h			;81e8	54		T
	nop			;81e9	00		.
	ld (bc),a		;81ea	02		.
	nop			;81eb	00		.
	xor d			;81ec	aa		.
	nop			;81ed	00		.
	xor d			;81ee	aa		.
	nop			;81ef	00		.
	xor b			;81f0	a8		.
	nop			;81f1	00		.
	nop			;81f2	00		.
	nop			;81f3	00		.
	nop			;81f4	00		.
	nop			;81f5	00		.
	nop			;81f6	00		.
	nop			;81f7	00		.
	nop			;81f8	00		.
	nop			;81f9	00		.
	nop			;81fa	00		.
	nop			;81fb	00		.
	nop			;81fc	00		.
	nop			;81fd	00		.
	nop			;81fe	00		.
	nop			;81ff	00		.
	nop			;8200	00		.
	nop			;8201	00		.
	nop			;8202	00		.
	nop			;8203	00		.
	nop			;8204	00		.
	nop			;8205	00		.
	nop			;8206	00		.
	nop			;8207	00		.
	nop			;8208	00		.
	nop			;8209	00		.
	nop			;820a	00		.
	nop			;820b	00		.
	nop			;820c	00		.
	nop			;820d	00		.
	nop			;820e	00		.
	nop			;820f	00		.
	nop			;8210	00		.
	nop			;8211	00		.
	nop			;8212	00		.
	nop			;8213	00		.
	nop			;8214	00		.
	nop			;8215	00		.
	nop			;8216	00		.
	nop			;8217	00		.
	nop			;8218	00		.
	nop			;8219	00		.
	nop			;821a	00		.
	nop			;821b	00		.
	nop			;821c	00		.
	nop			;821d	00		.
	nop			;821e	00		.
	nop			;821f	00		.
	nop			;8220	00		.
	nop			;8221	00		.
	nop			;8222	00		.
	nop			;8223	00		.
	nop			;8224	00		.
	nop			;8225	00		.
	nop			;8226	00		.
	nop			;8227	00		.
	nop			;8228	00		.
	nop			;8229	00		.
	nop			;822a	00		.
	nop			;822b	00		.
	nop			;822c	00		.
	nop			;822d	00		.
	nop			;822e	00		.
	nop			;822f	00		.
	nop			;8230	00		.
	nop			;8231	00		.
	nop			;8232	00		.
	nop			;8233	00		.
	nop			;8234	00		.
	nop			;8235	00		.
	nop			;8236	00		.
	nop			;8237	00		.
	nop			;8238	00		.
	nop			;8239	00		.
	nop			;823a	00		.
	nop			;823b	00		.
	nop			;823c	00		.
	nop			;823d	00		.
	nop			;823e	00		.
	nop			;823f	00		.
	nop			;8240	00		.
	nop			;8241	00		.
	nop			;8242	00		.
	nop			;8243	00		.
	nop			;8244	00		.
	nop			;8245	00		.
	nop			;8246	00		.
	nop			;8247	00		.
	nop			;8248	00		.
	nop			;8249	00		.
	nop			;824a	00		.
	nop			;824b	00		.
	nop			;824c	00		.
	nop			;824d	00		.
	nop			;824e	00		.
	nop			;824f	00		.
	nop			;8250	00		.
	nop			;8251	00		.
	nop			;8252	00		.
	nop			;8253	00		.
	nop			;8254	00		.
	nop			;8255	00		.
	nop			;8256	00		.
	nop			;8257	00		.
	nop			;8258	00		.
	nop			;8259	00		.
	nop			;825a	00		.
	nop			;825b	00		.
	nop			;825c	00		.
	nop			;825d	00		.
	nop			;825e	00		.
	nop			;825f	00		.
	nop			;8260	00		.
	nop			;8261	00		.
	nop			;8262	00		.
	nop			;8263	00		.
	nop			;8264	00		.
	nop			;8265	00		.
	nop			;8266	00		.
	nop			;8267	00		.
	nop			;8268	00		.
	nop			;8269	00		.
	nop			;826a	00		.
	nop			;826b	00		.
	nop			;826c	00		.
	nop			;826d	00		.
	nop			;826e	00		.
	nop			;826f	00		.
	nop			;8270	00		.
	nop			;8271	00		.
	nop			;8272	00		.
	nop			;8273	00		.
	nop			;8274	00		.
	nop			;8275	00		.
	nop			;8276	00		.
	nop			;8277	00		.
	nop			;8278	00		.
	nop			;8279	00		.
	nop			;827a	00		.
	nop			;827b	00		.
	nop			;827c	00		.
	nop			;827d	00		.
	nop			;827e	00		.
	nop			;827f	00		.
	nop			;8280	00		.
	nop			;8281	00		.
	nop			;8282	00		.
	nop			;8283	00		.
	nop			;8284	00		.
	nop			;8285	00		.
	nop			;8286	00		.
	nop			;8287	00		.
	nop			;8288	00		.
	nop			;8289	00		.
	nop			;828a	00		.
	nop			;828b	00		.
	nop			;828c	00		.
	nop			;828d	00		.
	nop			;828e	00		.
	nop			;828f	00		.
	nop			;8290	00		.
	nop			;8291	00		.
	nop			;8292	00		.
	nop			;8293	00		.
	nop			;8294	00		.
	nop			;8295	00		.
	nop			;8296	00		.
	nop			;8297	00		.
	nop			;8298	00		.
	nop			;8299	00		.
	nop			;829a	00		.
	nop			;829b	00		.
	nop			;829c	00		.
	nop			;829d	00		.
	nop			;829e	00		.
	nop			;829f	00		.
	nop			;82a0	00		.
	nop			;82a1	00		.
	nop			;82a2	00		.
	nop			;82a3	00		.
	nop			;82a4	00		.
	nop			;82a5	00		.
	nop			;82a6	00		.
	nop			;82a7	00		.
	nop			;82a8	00		.
	nop			;82a9	00		.
	nop			;82aa	00		.
	nop			;82ab	00		.
	nop			;82ac	00		.
	nop			;82ad	00		.
	nop			;82ae	00		.
	nop			;82af	00		.
	nop			;82b0	00		.
	nop			;82b1	00		.
	nop			;82b2	00		.
	nop			;82b3	00		.
	nop			;82b4	00		.
	nop			;82b5	00		.
	nop			;82b6	00		.
	nop			;82b7	00		.
	nop			;82b8	00		.
	nop			;82b9	00		.
	nop			;82ba	00		.
	nop			;82bb	00		.
	nop			;82bc	00		.
	nop			;82bd	00		.
	nop			;82be	00		.
	nop			;82bf	00		.
	nop			;82c0	00		.
	nop			;82c1	00		.
	nop			;82c2	00		.
	nop			;82c3	00		.
	nop			;82c4	00		.
	nop			;82c5	00		.
	nop			;82c6	00		.
	nop			;82c7	00		.
	nop			;82c8	00		.
	nop			;82c9	00		.
	nop			;82ca	00		.
	nop			;82cb	00		.
	nop			;82cc	00		.
	nop			;82cd	00		.
	nop			;82ce	00		.
	nop			;82cf	00		.
	nop			;82d0	00		.
	nop			;82d1	00		.
	nop			;82d2	00		.
	nop			;82d3	00		.
	nop			;82d4	00		.
	nop			;82d5	00		.
	nop			;82d6	00		.
	nop			;82d7	00		.
	nop			;82d8	00		.
	nop			;82d9	00		.
	nop			;82da	00		.
	nop			;82db	00		.
	nop			;82dc	00		.
	nop			;82dd	00		.
	nop			;82de	00		.
	nop			;82df	00		.
	nop			;82e0	00		.
	nop			;82e1	00		.
	nop			;82e2	00		.
	nop			;82e3	00		.
	nop			;82e4	00		.
	nop			;82e5	00		.
	nop			;82e6	00		.
	nop			;82e7	00		.
	nop			;82e8	00		.
	nop			;82e9	00		.
	nop			;82ea	00		.
	nop			;82eb	00		.
	nop			;82ec	00		.
	nop			;82ed	00		.
	nop			;82ee	00		.
	nop			;82ef	00		.
	nop			;82f0	00		.
	nop			;82f1	00		.
	nop			;82f2	00		.
	nop			;82f3	00		.
	nop			;82f4	00		.
	nop			;82f5	00		.
	nop			;82f6	00		.
	nop			;82f7	00		.
	nop			;82f8	00		.
	nop			;82f9	00		.
	nop			;82fa	00		.
	nop			;82fb	00		.
	nop			;82fc	00		.
	nop			;82fd	00		.
	nop			;82fe	00		.
	nop			;82ff	00		.
	nop			;8300	00		.
	nop			;8301	00		.
	nop			;8302	00		.
	nop			;8303	00		.
	nop			;8304	00		.
	nop			;8305	00		.
	nop			;8306	00		.
	nop			;8307	00		.
	nop			;8308	00		.
	nop			;8309	00		.
	nop			;830a	00		.
	nop			;830b	00		.
	nop			;830c	00		.
	nop			;830d	00		.
	nop			;830e	00		.
	nop			;830f	00		.
	nop			;8310	00		.
	nop			;8311	00		.
	nop			;8312	00		.
	nop			;8313	00		.
	nop			;8314	00		.
	nop			;8315	00		.
	nop			;8316	00		.
	nop			;8317	00		.
	nop			;8318	00		.
	nop			;8319	00		.
	nop			;831a	00		.
	nop			;831b	00		.
	nop			;831c	00		.
	nop			;831d	00		.
	nop			;831e	00		.
	nop			;831f	00		.
	nop			;8320	00		.
	nop			;8321	00		.
	nop			;8322	00		.
	nop			;8323	00		.
	nop			;8324	00		.
	nop			;8325	00		.
	nop			;8326	00		.
	nop			;8327	00		.
	nop			;8328	00		.
	nop			;8329	00		.
	nop			;832a	00		.
	nop			;832b	00		.
	nop			;832c	00		.
	nop			;832d	00		.
	nop			;832e	00		.
	nop			;832f	00		.
	nop			;8330	00		.
	nop			;8331	00		.
	nop			;8332	00		.
	nop			;8333	00		.
	nop			;8334	00		.
	nop			;8335	00		.
	nop			;8336	00		.
	nop			;8337	00		.
	nop			;8338	00		.
	nop			;8339	00		.
	nop			;833a	00		.
	nop			;833b	00		.
	nop			;833c	00		.
	nop			;833d	00		.
	nop			;833e	00		.
	nop			;833f	00		.
	nop			;8340	00		.
	nop			;8341	00		.
	ld (bc),a		;8342	02		.
	dec bc			;8343	0b		.
	djnz l8346h		;8344	10 00		. .
l8346h:
	nop			;8346	00		.
	nop			;8347	00		.
	jr c,l835ah		;8348	38 10		8 .
	nop			;834a	00		.
	nop			;834b	00		.
	ld a,h			;834c	7c		|
	djnz l834fh		;834d	10 00		. .
l834fh:
	nop			;834f	00		.
	cp 07ch			;8350	fe 7c		. |
	nop			;8352	00		.
	nop			;8353	00		.
	ld a,h			;8354	7c		|
	djnz l8357h		;8355	10 00		. .
l8357h:
	nop			;8357	00		.
	jr c,l836ah		;8358	38 10		8 .
l835ah:
	nop			;835a	00		.
	nop			;835b	00		.
	djnz l835eh		;835c	10 00		. .
l835eh:
	add a,b			;835e	80		.
	nop			;835f	00		.
	nop			;8360	00		.
	nop			;8361	00		.
	add a,b			;8362	80		.
	nop			;8363	00		.
	inc bc			;8364	03		.
	nop			;8365	00		.
	ret po			;8366	e0		.
	nop			;8367	00		.
	nop			;8368	00		.
	nop			;8369	00		.
l836ah:
	add a,b			;836a	80		.
	nop			;836b	00		.
	nop			;836c	00		.
	nop			;836d	00		.
	add a,b			;836e	80		.
	nop			;836f	00		.
	ld bc,0200ah		;8370	01 0a 20	. .  
	nop			;8373	00		.
	ld (hl),b		;8374	70		p
	jr nz,l83e7h		;8375	20 70		  p
	jr nz,$-6		;8377	20 f8		  .
	ld (hl),b		;8379	70		p
	ld (hl),b		;837a	70		p
	jr nz,l83efh		;837b	20 72		  r
	jr nz,l83a1h		;837d	20 22		  "
	nop			;837f	00		.
	rlca			;8380	07		.
	nop			;8381	00		.
	ld (bc),a		;8382	02		.
	nop			;8383	00		.
	ld (bc),a		;8384	02		.
l8385h:
	nop			;8385	00		.
	ld bc,02008h		;8386	01 08 20	. .  
	nop			;8389	00		.
	ld (hl),b		;838a	70		p
	jr nz,l8385h		;838b	20 f8		  .
	ld (hl),b		;838d	70		p
	ld (hl),b		;838e	70		p
	jr nz,l83b1h		;838f	20 20		   
	nop			;8391	00		.
	ld (bc),a		;8392	02		.
	nop			;8393	00		.
	rlca			;8394	07		.
	nop			;8395	00		.
	ld (bc),a		;8396	02		.
	nop			;8397	00		.
	ld bc,04006h		;8398	01 06 40	. . @
	nop			;839b	00		.
	ret po			;839c	e0		.
	ld b,b			;839d	40		@
	ret po			;839e	e0		.
	ld b,b			;839f	40		@
	ld b,b			;83a0	40		@
l83a1h:
	nop			;83a1	00		.
	ex af,af'		;83a2	08		.
	nop			;83a3	00		.
	ex af,af'		;83a4	08		.
	nop			;83a5	00		.
	ld bc,04004h		;83a6	01 04 40	. . @
	nop			;83a9	00		.
	ret po			;83aa	e0		.
	ld b,b			;83ab	40		@
	ld b,b			;83ac	40		@
	nop			;83ad	00		.
	djnz l83b0h		;83ae	10 00		. .
l83b0h:
	inc bc			;83b0	03		.
l83b1h:
	ld c,000h		;83b1	0e 00		. .
	nop			;83b3	00		.
	ld a,(hl)		;83b4	7e		~
	nop			;83b5	00		.
	nop			;83b6	00		.
	nop			;83b7	00		.
	inc bc			;83b8	03		.
	nop			;83b9	00		.
	rst 38h			;83ba	ff		.
	ld e,(hl)		;83bb	5e		^
	ret nz			;83bc	c0		.
	nop			;83bd	00		.
	rrca			;83be	0f		.
	inc bc			;83bf	03		.
	rst 38h			;83c0	ff		.
	sbc a,a			;83c1	9f		.
	ret p			;83c2	f0		.
	ret nz			;83c3	c0		.
	ccf			;83c4	3f		?
	ld c,0ffh		;83c5	0e ff		. .
	ccf			;83c7	3f		?
	call m,sub_7ff0h	;83c8	fc f0 7f	. . .
	jr c,$+1		;83cb	38 ff		8 .
	ld a,a			;83cd	7f		.
	cp 0fch			;83ce	fe fc		. .
	rst 38h			;83d0	ff		.
	ld (hl),b		;83d1	70		p
	rst 38h			;83d2	ff		.
	rst 38h			;83d3	ff		.
	rst 38h			;83d4	ff		.
	cp 0ffh			;83d5	fe ff		. .
	ld h,c			;83d7	61		a
	rst 38h			;83d8	ff		.
	rst 38h			;83d9	ff		.
	rst 38h			;83da	ff		.
	cp 07fh			;83db	fe 7f		. .
	nop			;83dd	00		.
	rst 38h			;83de	ff		.
	nop			;83df	00		.
	cp 000h			;83e0	fe 00		. .
	rst 38h			;83e2	ff		.
	ld a,c			;83e3	79		y
	rst 38h			;83e4	ff		.
	rst 20h			;83e5	e7		.
	rst 38h			;83e6	ff		.
l83e7h:
	sbc a,(hl)		;83e7	9e		.
	rst 38h			;83e8	ff		.
	ld a,c			;83e9	79		y
	rst 38h			;83ea	ff		.
	rst 20h			;83eb	e7		.
	rst 38h			;83ec	ff		.
	sbc a,(hl)		;83ed	9e		.
	rst 38h			;83ee	ff		.
l83efh:
	ld a,c			;83ef	79		y
	rst 38h			;83f0	ff		.
	rst 20h			;83f1	e7		.
	rst 38h			;83f2	ff		.
	sbc a,(hl)		;83f3	9e		.
	ld a,a			;83f4	7f		.
	nop			;83f5	00		.
	rst 38h			;83f6	ff		.
	nop			;83f7	00		.
	cp 000h			;83f8	fe 00		. .
	rst 38h			;83fa	ff		.
	ld a,a			;83fb	7f		.
	rst 38h			;83fc	ff		.
	rst 18h			;83fd	df		.
	rst 38h			;83fe	ff		.
	cp 07fh			;83ff	fe 7f		. .
	nop			;8401	00		.
	rst 38h			;8402	ff		.
	nop			;8403	00		.
	cp 000h			;8404	fe 00		. .
	inc bc			;8406	03		.
	rrca			;8407	0f		.
	nop			;8408	00		.
	nop			;8409	00		.
	nop			;840a	00		.
	nop			;840b	00		.
	nop			;840c	00		.
	nop			;840d	00		.
	nop			;840e	00		.
	nop			;840f	00		.
	ld a,(hl)		;8410	7e		~
	nop			;8411	00		.
	nop			;8412	00		.
	nop			;8413	00		.
	inc bc			;8414	03		.
	nop			;8415	00		.
	rst 38h			;8416	ff		.
	ld a,0c0h		;8417	3e c0		> .
	nop			;8419	00		.
	rrca			;841a	0f		.
	inc bc			;841b	03		.
	rst 38h			;841c	ff		.
	ccf			;841d	3f		?
	ret p			;841e	f0		.
	ret nz			;841f	c0		.
	ccf			;8420	3f		?
	ld c,0ffh		;8421	0e ff		. .
	ld a,a			;8423	7f		.
	call m,sub_7ff0h	;8424	fc f0 7f	. . .
	jr c,$+1		;8427	38 ff		8 .
	ld a,a			;8429	7f		.
	cp 0fch			;842a	fe fc		. .
	rst 38h			;842c	ff		.
	ld (hl),b		;842d	70		p
	rst 38h			;842e	ff		.
	rst 38h			;842f	ff		.
	rst 38h			;8430	ff		.
	cp 07fh			;8431	fe 7f		. .
	nop			;8433	00		.
	rst 38h			;8434	ff		.
	nop			;8435	00		.
	cp 000h			;8436	fe 00		. .
	ld a,a			;8438	7f		.
	inc a			;8439	3c		<
	rst 38h			;843a	ff		.
	di			;843b	f3		.
	rst 38h			;843c	ff		.
	adc a,07fh		;843d	ce 7f		. .
	inc a			;843f	3c		<
	rst 38h			;8440	ff		.
	di			;8441	f3		.
	rst 38h			;8442	ff		.
	adc a,07fh		;8443	ce 7f		. .
	inc a			;8445	3c		<
	rst 38h			;8446	ff		.
	di			;8447	f3		.
	rst 38h			;8448	ff		.
	adc a,07fh		;8449	ce 7f		. .
	nop			;844b	00		.
	rst 38h			;844c	ff		.
	nop			;844d	00		.
	cp 000h			;844e	fe 00		. .
	rst 38h			;8450	ff		.
	ld a,(hl)		;8451	7e		~
	rst 38h			;8452	ff		.
	ld a,a			;8453	7f		.
	rst 38h			;8454	ff		.
	cp 07fh			;8455	fe 7f		. .
	rrca			;8457	0f		.
	rst 38h			;8458	ff		.
	ccf			;8459	3f		?
	cp 0e0h			;845a	fe e0		. .
	rlca			;845c	07		.
	nop			;845d	00		.
	rst 38h			;845e	ff		.
	nop			;845f	00		.
	ret po			;8460	e0		.
	nop			;8461	00		.
	inc bc			;8462	03		.
	djnz l8465h		;8463	10 00		. .
l8465h:
	nop			;8465	00		.
	nop			;8466	00		.
	nop			;8467	00		.
	nop			;8468	00		.
	nop			;8469	00		.
	nop			;846a	00		.
	nop			;846b	00		.
	nop			;846c	00		.
	nop			;846d	00		.
	nop			;846e	00		.
	nop			;846f	00		.
	nop			;8470	00		.
	nop			;8471	00		.
	ld a,(hl)		;8472	7e		~
	nop			;8473	00		.
	nop			;8474	00		.
	nop			;8475	00		.
	inc bc			;8476	03		.
	nop			;8477	00		.
	rst 38h			;8478	ff		.
	ld a,0c0h		;8479	3e c0		> .
	nop			;847b	00		.
	rra			;847c	1f		.
	inc bc			;847d	03		.
	rst 38h			;847e	ff		.
	ccf			;847f	3f		?
	ret m			;8480	f8		.
	ret nz			;8481	c0		.
	ld a,a			;8482	7f		.
	ld e,0ffh		;8483	1e ff		. .
	ccf			;8485	3f		?
	cp 0f8h			;8486	fe f8		. .
	rst 38h			;8488	ff		.
	ld a,b			;8489	78		x
	rst 38h			;848a	ff		.
	ld a,a			;848b	7f		.
	rst 38h			;848c	ff		.
	cp 07fh			;848d	fe 7f		. .
	nop			;848f	00		.
	rst 38h			;8490	ff		.
	nop			;8491	00		.
	cp 000h			;8492	fe 00		. .
	ld a,a			;8494	7f		.
	ld e,0ffh		;8495	1e ff		. .
	ld a,c			;8497	79		y
	rst 38h			;8498	ff		.
	and 07fh		;8499	e6 7f		. .
	ld e,0ffh		;849b	1e ff		. .
	ld a,c			;849d	79		y
	rst 38h			;849e	ff		.
	and 07fh		;849f	e6 7f		. .
	ld e,0ffh		;84a1	1e ff		. .
	ld a,c			;84a3	79		y
	rst 38h			;84a4	ff		.
	and 07fh		;84a5	e6 7f		. .
	nop			;84a7	00		.
	rst 38h			;84a8	ff		.
	nop			;84a9	00		.
	cp 000h			;84aa	fe 00		. .
	rst 38h			;84ac	ff		.
	ld a,h			;84ad	7c		|
	rst 38h			;84ae	ff		.
	rst 38h			;84af	ff		.
	rst 38h			;84b0	ff		.
	cp 07fh			;84b1	fe 7f		. .
	ld e,0ffh		;84b3	1e ff		. .
	ld a,a			;84b5	7f		.
	cp 0f8h			;84b6	fe f8		. .
	rra			;84b8	1f		.
	ld bc,l7fffh		;84b9	01 ff 7f	. . .
	ret m			;84bc	f8		.
	add a,b			;84bd	80		.
	ld bc,0ff00h		;84be	01 00 ff	. . .
	nop			;84c1	00		.
	add a,b			;84c2	80		.
	nop			;84c3	00		.
	inc bc			;84c4	03		.
	ld de,00000h		;84c5	11 00 00	. . .
	nop			;84c8	00		.
	nop			;84c9	00		.
	nop			;84ca	00		.
	nop			;84cb	00		.
	nop			;84cc	00		.
	nop			;84cd	00		.
	nop			;84ce	00		.
	nop			;84cf	00		.
	nop			;84d0	00		.
	nop			;84d1	00		.
	nop			;84d2	00		.
	nop			;84d3	00		.
	nop			;84d4	00		.
	nop			;84d5	00		.
	nop			;84d6	00		.
	nop			;84d7	00		.
	ld bc,0ff00h		;84d8	01 00 ff	. . .
	nop			;84db	00		.
	add a,b			;84dc	80		.
	nop			;84dd	00		.
	rra			;84de	1f		.
	ld bc,l7fffh		;84df	01 ff 7f	. . .
	ret m			;84e2	f8		.
	add a,b			;84e3	80		.
	ld a,a			;84e4	7f		.
	ld e,0ffh		;84e5	1e ff		. .
	ld a,a			;84e7	7f		.
	cp 0f8h			;84e8	fe f8		. .
	rst 38h			;84ea	ff		.
	ld a,h			;84eb	7c		|
	rst 38h			;84ec	ff		.
	rst 38h			;84ed	ff		.
	rst 38h			;84ee	ff		.
	cp 07fh			;84ef	fe 7f		. .
	nop			;84f1	00		.
	rst 38h			;84f2	ff		.
	nop			;84f3	00		.
	cp 000h			;84f4	fe 00		. .
	rst 38h			;84f6	ff		.
	ld c,a			;84f7	4f		O
	rst 38h			;84f8	ff		.
	inc a			;84f9	3c		<
	rst 38h			;84fa	ff		.
	jp p,04fffh		;84fb	f2 ff 4f	. . O
	rst 38h			;84fe	ff		.
	inc a			;84ff	3c		<
	rst 38h			;8500	ff		.
	jp p,04fffh		;8501	f2 ff 4f	. . O
	rst 38h			;8504	ff		.
	inc a			;8505	3c		<
	rst 38h			;8506	ff		.
	jp p,0007fh		;8507	f2 7f 00	. . .
	rst 38h			;850a	ff		.
	nop			;850b	00		.
	cp 000h			;850c	fe 00		. .
	rst 38h			;850e	ff		.
	ld a,b			;850f	78		x
	rst 38h			;8510	ff		.
	ld a,a			;8511	7f		.
	rst 38h			;8512	ff		.
	cp 07fh			;8513	fe 7f		. .
	ld e,0ffh		;8515	1e ff		. .
	ccf			;8517	3f		?
	cp 0f8h			;8518	fe f8		. .
	rra			;851a	1f		.
	inc bc			;851b	03		.
	rst 38h			;851c	ff		.
	ccf			;851d	3f		?
	ret m			;851e	f8		.
	ret nz			;851f	c0		.
	inc bc			;8520	03		.
	nop			;8521	00		.
	rst 38h			;8522	ff		.
	ccf			;8523	3f		?
	ret nz			;8524	c0		.
	nop			;8525	00		.
	nop			;8526	00		.
	nop			;8527	00		.
	ld a,(hl)		;8528	7e		~
	nop			;8529	00		.
	nop			;852a	00		.
	nop			;852b	00		.
	inc bc			;852c	03		.
	ld (de),a		;852d	12		.
	nop			;852e	00		.
	nop			;852f	00		.
	nop			;8530	00		.
	nop			;8531	00		.
	nop			;8532	00		.
	nop			;8533	00		.
	nop			;8534	00		.
	nop			;8535	00		.
	nop			;8536	00		.
	nop			;8537	00		.
	nop			;8538	00		.
	nop			;8539	00		.
	nop			;853a	00		.
	nop			;853b	00		.
	nop			;853c	00		.
	nop			;853d	00		.
	nop			;853e	00		.
	nop			;853f	00		.
	nop			;8540	00		.
	nop			;8541	00		.
	nop			;8542	00		.
	nop			;8543	00		.
	nop			;8544	00		.
	nop			;8545	00		.
	rlca			;8546	07		.
	nop			;8547	00		.
	rst 38h			;8548	ff		.
	nop			;8549	00		.
	ret po			;854a	e0		.
	nop			;854b	00		.
	ld a,a			;854c	7f		.
	rlca			;854d	07		.
	rst 38h			;854e	ff		.
	ccf			;854f	3f		?
	cp 0e0h			;8550	fe e0		. .
	rst 38h			;8552	ff		.
	ld a,(hl)		;8553	7e		~
	rst 38h			;8554	ff		.
	ld a,a			;8555	7f		.
	rst 38h			;8556	ff		.
	cp 07fh			;8557	fe 7f		. .
	nop			;8559	00		.
	rst 38h			;855a	ff		.
	nop			;855b	00		.
	cp 000h			;855c	fe 00		. .
	rst 38h			;855e	ff		.
	ld h,a			;855f	67		g
	rst 38h			;8560	ff		.
	sbc a,(hl)		;8561	9e		.
	cp 078h			;8562	fe 78		. x
	rst 38h			;8564	ff		.
	ld h,a			;8565	67		g
	rst 38h			;8566	ff		.
	sbc a,(hl)		;8567	9e		.
	cp 078h			;8568	fe 78		. x
	rst 38h			;856a	ff		.
	ld h,a			;856b	67		g
	rst 38h			;856c	ff		.
	sbc a,(hl)		;856d	9e		.
	cp 078h			;856e	fe 78		. x
	ld a,a			;8570	7f		.
	nop			;8571	00		.
	rst 38h			;8572	ff		.
	nop			;8573	00		.
	cp 000h			;8574	fe 00		. .
	rst 38h			;8576	ff		.
	ld (hl),b		;8577	70		p
	rst 38h			;8578	ff		.
	rst 38h			;8579	ff		.
	rst 38h			;857a	ff		.
	cp 07fh			;857b	fe 7f		. .
	jr c,$+1		;857d	38 ff		8 .
	ld a,a			;857f	7f		.
	cp 0fch			;8580	fe fc		. .
	ccf			;8582	3f		?
	ld c,0ffh		;8583	0e ff		. .
	ld a,a			;8585	7f		.
	call m,00ff0h		;8586	fc f0 0f	. . .
	inc bc			;8589	03		.
	rst 38h			;858a	ff		.
	ccf			;858b	3f		?
	ret p			;858c	f0		.
	ret nz			;858d	c0		.
	inc bc			;858e	03		.
	nop			;858f	00		.
	rst 38h			;8590	ff		.
	ld a,0c0h		;8591	3e c0		> .
	nop			;8593	00		.
	nop			;8594	00		.
	nop			;8595	00		.
	ld a,(hl)		;8596	7e		~
	nop			;8597	00		.
	nop			;8598	00		.
	nop			;8599	00		.
	inc bc			;859a	03		.
	inc de			;859b	13		.
	nop			;859c	00		.
	nop			;859d	00		.
	nop			;859e	00		.
	nop			;859f	00		.
	nop			;85a0	00		.
	nop			;85a1	00		.
	nop			;85a2	00		.
	nop			;85a3	00		.
	nop			;85a4	00		.
	nop			;85a5	00		.
	nop			;85a6	00		.
	nop			;85a7	00		.
	nop			;85a8	00		.
	nop			;85a9	00		.
	nop			;85aa	00		.
	nop			;85ab	00		.
	nop			;85ac	00		.
	nop			;85ad	00		.
	nop			;85ae	00		.
	nop			;85af	00		.
	nop			;85b0	00		.
	nop			;85b1	00		.
	nop			;85b2	00		.
	nop			;85b3	00		.
	nop			;85b4	00		.
	nop			;85b5	00		.
	nop			;85b6	00		.
	nop			;85b7	00		.
	nop			;85b8	00		.
	nop			;85b9	00		.
	ld a,a			;85ba	7f		.
	nop			;85bb	00		.
	rst 38h			;85bc	ff		.
	nop			;85bd	00		.
	cp 000h			;85be	fe 00		. .
	rst 38h			;85c0	ff		.
	ld a,a			;85c1	7f		.
	rst 38h			;85c2	ff		.
	rst 18h			;85c3	df		.
	rst 38h			;85c4	ff		.
	cp 07fh			;85c5	fe 7f		. .
	nop			;85c7	00		.
	rst 38h			;85c8	ff		.
	nop			;85c9	00		.
	cp 000h			;85ca	fe 00		. .
	rst 38h			;85cc	ff		.
	ld (hl),e		;85cd	73		s
	rst 38h			;85ce	ff		.
	rst 8			;85cf	cf		.
	cp 03ch			;85d0	fe 3c		. <
	rst 38h			;85d2	ff		.
	ld (hl),e		;85d3	73		s
	rst 38h			;85d4	ff		.
	rst 8			;85d5	cf		.
	cp 03ch			;85d6	fe 3c		. <
	rst 38h			;85d8	ff		.
	ld (hl),e		;85d9	73		s
	rst 38h			;85da	ff		.
	rst 8			;85db	cf		.
	cp 03ch			;85dc	fe 3c		. <
	ld a,a			;85de	7f		.
	nop			;85df	00		.
	rst 38h			;85e0	ff		.
	nop			;85e1	00		.
	cp 000h			;85e2	fe 00		. .
	rst 38h			;85e4	ff		.
	ld h,c			;85e5	61		a
	rst 38h			;85e6	ff		.
	rst 38h			;85e7	ff		.
	rst 38h			;85e8	ff		.
	cp 0ffh			;85e9	fe ff		. .
	ld (hl),b		;85eb	70		p
	rst 38h			;85ec	ff		.
	rst 38h			;85ed	ff		.
	rst 38h			;85ee	ff		.
	cp 07fh			;85ef	fe 7f		. .
	jr c,$+1		;85f1	38 ff		8 .
	ld a,a			;85f3	7f		.
	cp 0fch			;85f4	fe fc		. .
	ccf			;85f6	3f		?
	ld c,0ffh		;85f7	0e ff		. .
	ccf			;85f9	3f		?
	call m,00ff0h		;85fa	fc f0 0f	. . .
	inc bc			;85fd	03		.
	rst 38h			;85fe	ff		.
	sbc a,a			;85ff	9f		.
	ret p			;8600	f0		.
	ret nz			;8601	c0		.
	inc bc			;8602	03		.
	nop			;8603	00		.
	rst 38h			;8604	ff		.
	ld e,(hl)		;8605	5e		^
	ret nz			;8606	c0		.
	nop			;8607	00		.
	nop			;8608	00		.
	nop			;8609	00		.
	ld a,(hl)		;860a	7e		~
	nop			;860b	00		.
	nop			;860c	00		.
	nop			;860d	00		.
	inc bc			;860e	03		.
	rrca			;860f	0f		.
	ret p			;8610	f0		.
	nop			;8611	00		.
	jr l8614h		;8612	18 00		. .
l8614h:
	ld c,000h		;8614	0e 00		. .
	cp 070h			;8616	fe 70		. p
	inc a			;8618	3c		<
	jr l869ah		;8619	18 7f		. .
	ld c,07fh		;861b	0e 7f		. .
	ld a,07eh		;861d	3e 7e		> ~
	inc a			;861f	3c		<
	cp 07ch			;8620	fe 7c		. |
	ccf			;8622	3f		?
	rra			;8623	1f		.
	rst 38h			;8624	ff		.
	ld e,d			;8625	5a		Z
	call m,01ff8h		;8626	fc f8 1f	. . .
	rlca			;8629	07		.
	rst 38h			;862a	ff		.
	ld h,(hl)		;862b	66		f
	ret m			;862c	f8		.
	ret po			;862d	e0		.
	rlca			;862e	07		.
	ld bc,l99ffh		;862f	01 ff 99	. . .
	pop hl			;8632	e1		.
	add a,b			;8633	80		.
	add hl,hl		;8634	29		)
	nop			;8635	00		.
	rst 38h			;8636	ff		.
	ld e,d			;8637	5a		Z
	adc a,d			;8638	8a		.
	nop			;8639	00		.
	dec d			;863a	15		.
	nop			;863b	00		.
	rst 38h			;863c	ff		.
	sbc a,c			;863d	99		.
	sub h			;863e	94		.
	nop			;863f	00		.
	add hl,bc		;8640	09		.
	nop			;8641	00		.
	rst 38h			;8642	ff		.
	sub c			;8643	91		.
	xor b			;8644	a8		.
	nop			;8645	00		.
	inc bc			;8646	03		.
	ld bc,00095h		;8647	01 95 00	. . .
	ret nc			;864a	d0		.
	add a,b			;864b	80		.
	ld bc,02a00h		;864c	01 00 2a	. . *
	nop			;864f	00		.
	and b			;8650	a0		.
	nop			;8651	00		.
	nop			;8652	00		.
	nop			;8653	00		.
	dec d			;8654	15		.
	nop			;8655	00		.
	ld d,b			;8656	50		P
	nop			;8657	00		.
	nop			;8658	00		.
	nop			;8659	00		.
	ex af,af'		;865a	08		.
	nop			;865b	00		.
	ex af,af'		;865c	08		.
	nop			;865d	00		.
	nop			;865e	00		.
	nop			;865f	00		.
	djnz l8662h		;8660	10 00		. .
l8662h:
	djnz l8664h		;8662	10 00		. .
l8664h:
	nop			;8664	00		.
	nop			;8665	00		.
	ex af,af'		;8666	08		.
	nop			;8667	00		.
	ex af,af'		;8668	08		.
	nop			;8669	00		.
	inc bc			;866a	03		.
	rrca			;866b	0f		.
	nop			;866c	00		.
	nop			;866d	00		.
	jr l8670h		;866e	18 00		. .
l8670h:
	nop			;8670	00		.
	nop			;8671	00		.
	nop			;8672	00		.
	nop			;8673	00		.
	inc a			;8674	3c		<
	jr l8677h		;8675	18 00		. .
l8677h:
	nop			;8677	00		.
	nop			;8678	00		.
	nop			;8679	00		.
	ld a,(hl)		;867a	7e		~
	inc a			;867b	3c		<
	nop			;867c	00		.
	nop			;867d	00		.
	ld bc,0ff00h		;867e	01 00 ff	. . .
	ld e,d			;8681	5a		Z
	add a,b			;8682	80		.
	nop			;8683	00		.
	rra			;8684	1f		.
	ld bc,l7effh		;8685	01 ff 7e	. . ~
	ret m			;8688	f8		.
	add a,b			;8689	80		.
	ccf			;868a	3f		?
	rra			;868b	1f		.
	rst 38h			;868c	ff		.
	and l			;868d	a5		.
	call m,sub_7ff8h	;868e	fc f8 7f	. . .
	ccf			;8691	3f		?
	rst 38h			;8692	ff		.
	ld e,d			;8693	5a		Z
	cp 0fch			;8694	fe fc		. .
	rst 38h			;8696	ff		.
	ld h,b			;8697	60		`
	rst 38h			;8698	ff		.
	ld e,d			;8699	5a		Z
l869ah:
	rst 38h			;869a	ff		.
	ld b,064h		;869b	06 64		. d
	nop			;869d	00		.
	rst 38h			;869e	ff		.
	ld c,d			;869f	4a		J
	ld d,(hl)		;86a0	56		V
	nop			;86a1	00		.
	dec bc			;86a2	0b		.
	nop			;86a3	00		.
	res 0,c			;86a4	cb 81		. .
	xor d			;86a6	aa		.
	nop			;86a7	00		.
	inc d			;86a8	14		.
	nop			;86a9	00		.
	sub l			;86aa	95		.
	nop			;86ab	00		.
	ld d,l			;86ac	55		U
	nop			;86ad	00		.
	ld (bc),a		;86ae	02		.
	nop			;86af	00		.
	xor d			;86b0	aa		.
	nop			;86b1	00		.
	ld c,d			;86b2	4a		J
	nop			;86b3	00		.
	ld bc,01100h		;86b4	01 00 11	. . .
	nop			;86b7	00		.
	jr nz,l86bah		;86b8	20 00		  .
l86bah:
	nop			;86ba	00		.
	nop			;86bb	00		.
	nop			;86bc	00		.
	nop			;86bd	00		.
	ld b,b			;86be	40		@
	nop			;86bf	00		.
	nop			;86c0	00		.
	nop			;86c1	00		.
	djnz l86c4h		;86c2	10 00		. .
l86c4h:
	jr nz,l86c6h		;86c4	20 00		  .
l86c6h:
	inc bc			;86c6	03		.
	rrca			;86c7	0f		.
	nop			;86c8	00		.
	nop			;86c9	00		.
	jr l86cch		;86ca	18 00		. .
l86cch:
	nop			;86cc	00		.
	nop			;86cd	00		.
	nop			;86ce	00		.
	nop			;86cf	00		.
	inc a			;86d0	3c		<
	jr l86d3h		;86d1	18 00		. .
l86d3h:
	nop			;86d3	00		.
	nop			;86d4	00		.
	nop			;86d5	00		.
	ld a,(hl)		;86d6	7e		~
	inc a			;86d7	3c		<
	nop			;86d8	00		.
	nop			;86d9	00		.
	ld bc,0ff00h		;86da	01 00 ff	. . .
	ld e,d			;86dd	5a		Z
	add a,b			;86de	80		.
	nop			;86df	00		.
	rlca			;86e0	07		.
	ld bc,l7effh		;86e1	01 ff 7e	. . ~
	ret po			;86e4	e0		.
	add a,b			;86e5	80		.
	rrca			;86e6	0f		.
	rlca			;86e7	07		.
	rst 38h			;86e8	ff		.
l86e9h:
	and l			;86e9	a5		.
	ret p			;86ea	f0		.
	ret po			;86eb	e0		.
	rra			;86ec	1f		.
	rrca			;86ed	0f		.
	rst 38h			;86ee	ff		.
l86efh:
	jr l86e9h		;86ef	18 f8		. .
	ret p			;86f1	f0		.
l86f2h:
	ccf			;86f2	3f		?
	inc e			;86f3	1c		.
	rst 38h			;86f4	ff		.
	ld e,d			;86f5	5a		Z
	call m,03c38h		;86f6	fc 38 3c	. 8 <
	jr $+1			;86f9	18 ff		. .
	ld c,d			;86fb	4a		J
	cp h			;86fc	bc		.
	jr l8778h		;86fd	18 79		. y
	jr nc,l86cch		;86ff	30 cb		0 .
	add a,c			;8701	81		.
	sbc a,(hl)		;8702	9e		.
	inc c			;8703	0c		.
	inc (hl)		;8704	34		4
	nop			;8705	00		.
	sub l			;8706	95		.
	nop			;8707	00		.
	ld c,h			;8708	4c		L
	nop			;8709	00		.
	ld a,(bc)		;870a	0a		.
	nop			;870b	00		.
	xor d			;870c	aa		.
	nop			;870d	00		.
	xor d			;870e	aa		.
	nop			;870f	00		.
	dec d			;8710	15		.
	nop			;8711	00		.
	ld d,l			;8712	55		U
	nop			;8713	00		.
	ld d,l			;8714	55		U
	nop			;8715	00		.
	nop			;8716	00		.
	nop			;8717	00		.
	jr nz,l871ah		;8718	20 00		  .
l871ah:
	add a,b			;871a	80		.
	nop			;871b	00		.
	nop			;871c	00		.
	nop			;871d	00		.
	jr nz,l8720h		;871e	20 00		  .
l8720h:
	add a,b			;8720	80		.
	nop			;8721	00		.
	inc bc			;8722	03		.
	rrca			;8723	0f		.
	nop			;8724	00		.
	nop			;8725	00		.
	jr l8728h		;8726	18 00		. .
l8728h:
	nop			;8728	00		.
	nop			;8729	00		.
l872ah:
	inc bc			;872a	03		.
	nop			;872b	00		.
	inc a			;872c	3c		<
	jr l86efh		;872d	18 c0		. .
	nop			;872f	00		.
	rlca			;8730	07		.
	inc bc			;8731	03		.
	rst 38h			;8732	ff		.
	inc a			;8733	3c		<
	ret po			;8734	e0		.
	ret nz			;8735	c0		.
	rrca			;8736	0f		.
	rlca			;8737	07		.
	rst 38h			;8738	ff		.
	ld e,d			;8739	5a		Z
	ret p			;873a	f0		.
	ret po			;873b	e0		.
	rra			;873c	1f		.
	ld c,0ffh		;873d	0e ff		. .
	and l			;873f	a5		.
	jp m,01e70h		;8740	fa 70 1e	. p .
	inc c			;8743	0c		.
	rst 38h			;8744	ff		.
	jr l87c0h		;8745	18 79		. y
	jr nc,l8767h		;8747	30 1e		0 .
	inc c			;8749	0c		.
	rst 38h			;874a	ff		.
	ld e,d			;874b	5a		Z
	ld a,d			;874c	7a		z
	jr nc,l878dh		;874d	30 3e		0 >
	inc e			;874f	1c		.
	rst 38h			;8750	ff		.
	ld d,d			;8751	52		R
	ld a,l			;8752	7d		}
	jr c,$+63		;8753	38 3d		8 =
	jr l872ah		;8755	18 d3		. .
	add a,c			;8757	81		.
	cp (hl)			;8758	be		.
	jr l8795h		;8759	18 3a		. :
	djnz l86f2h		;875b	10 95		. .
	nop			;875d	00		.
	ld e,l			;875e	5d		]
	ex af,af'		;875f	08		.
	dec d			;8760	15		.
	nop			;8761	00		.
	ld hl,(la9ffh+1)	;8762	2a 00 aa	* . .
	nop			;8765	00		.
	ld a,(bc)		;8766	0a		.
l8767h:
	nop			;8767	00		.
	ld b,l			;8768	45		E
	nop			;8769	00		.
	ld d,c			;876a	51		Q
	nop			;876b	00		.
	inc b			;876c	04		.
	nop			;876d	00		.
	jr nz,l8770h		;876e	20 00		  .
l8770h:
	jr nz,l8772h		;8770	20 00		  .
l8772h:
	nop			;8772	00		.
	nop			;8773	00		.
	djnz l8776h		;8774	10 00		. .
l8776h:
	ld b,b			;8776	40		@
	nop			;8777	00		.
l8778h:
	inc bc			;8778	03		.
	ld (de),a		;8779	12		.
	jr nc,l877ch		;877a	30 00		0 .
l877ch:
	nop			;877c	00		.
	nop			;877d	00		.
	inc c			;877e	0c		.
	nop			;877f	00		.
	ld a,b			;8780	78		x
	jr nc,l8783h		;8781	30 00		0 .
l8783h:
	nop			;8783	00		.
	ld e,00ch		;8784	1e 0c		. .
	inc a			;8786	3c		<
	jr l8789h		;8787	18 00		. .
l8789h:
	nop			;8789	00		.
	inc a			;878a	3c		<
	jr l87abh		;878b	18 1e		. .
l878dh:
	inc c			;878d	0c		.
	jr l8790h		;878e	18 00		. .
l8790h:
	ld a,b			;8790	78		x
	jr nc,l87b2h		;8791	30 1f		0 .
	ld c,03ch		;8793	0e 3c		. <
l8795h:
	jr l8790h		;8795	18 f9		. .
	ld (hl),b		;8797	70		p
	cpl			;8798	2f		/
	rlca			;8799	07		.
	rst 38h			;879a	ff		.
	inc a			;879b	3c		<
	jp p,017e0h		;879c	f2 e0 17	. . .
	inc bc			;879f	03		.
	rst 38h			;87a0	ff		.
	ld e,d			;87a1	5a		Z
	push hl			;87a2	e5		.
	ret nz			;87a3	c0		.
	cpl			;87a4	2f		/
	inc bc			;87a5	03		.
	rst 38h			;87a6	ff		.
	ld a,(hl)		;87a7	7e		~
	jp pe,013c0h		;87a8	ea c0 13	. . .
l87abh:
	ld bc,la5ffh		;87ab	01 ff a5	. . .
	call nc,00980h		;87ae	d4 80 09	. . .
	nop			;87b1	00		.
l87b2h:
	rst 38h			;87b2	ff		.
	ld e,d			;87b3	5a		Z
	xor b			;87b4	a8		.
	nop			;87b5	00		.
	dec b			;87b6	05		.
	nop			;87b7	00		.
	ld a,(hl)		;87b8	7e		~
	jr $+82			;87b9	18 50		. P
	nop			;87bb	00		.
	ld (bc),a		;87bc	02		.
	nop			;87bd	00		.
	rst 38h			;87be	ff		.
	ld d,d			;87bf	52		R
l87c0h:
	jr z,l87c2h		;87c0	28 00		( .
l87c2h:
	ld bc,0d300h		;87c2	01 00 d3	. . .
	add a,c			;87c5	81		.
	ret nc			;87c6	d0		.
	nop			;87c7	00		.
	ld bc,0d700h		;87c8	01 00 d7	. . .
	add a,c			;87cb	81		.
	and b			;87cc	a0		.
	nop			;87cd	00		.
	nop			;87ce	00		.
	nop			;87cf	00		.
	xor c			;87d0	a9		.
	nop			;87d1	00		.
	ld b,b			;87d2	40		@
	nop			;87d3	00		.
	nop			;87d4	00		.
	nop			;87d5	00		.
	nop			;87d6	00		.
	nop			;87d7	00		.
	nop			;87d8	00		.
	nop			;87d9	00		.
	nop			;87da	00		.
	nop			;87db	00		.
	jr nz,l87deh		;87dc	20 00		  .
l87deh:
	ld b,b			;87de	40		@
	nop			;87df	00		.
	nop			;87e0	00		.
	nop			;87e1	00		.
	djnz l87e4h		;87e2	10 00		. .
l87e4h:
	jr nz,l87e6h		;87e4	20 00		  .
l87e6h:
	ld (bc),a		;87e6	02		.
	dec c			;87e7	0d		.
	nop			;87e8	00		.
	nop			;87e9	00		.
	nop			;87ea	00		.
	nop			;87eb	00		.
	nop			;87ec	00		.
	nop			;87ed	00		.
	nop			;87ee	00		.
	nop			;87ef	00		.
	nop			;87f0	00		.
	nop			;87f1	00		.
	nop			;87f2	00		.
	nop			;87f3	00		.
	ld (bc),a		;87f4	02		.
	nop			;87f5	00		.
	ld b,b			;87f6	40		@
	nop			;87f7	00		.
	inc c			;87f8	0c		.
	nop			;87f9	00		.
	ret pe			;87fa	e8		.
	ld b,b			;87fb	40		@
	rra			;87fc	1f		.
	inc c			;87fd	0c		.
	ret po			;87fe	e0		.
	ret nz			;87ff	c0		.
	rra			;8800	1f		.
	rrca			;8801	0f		.
	ret p			;8802	f0		.
	ret nz			;8803	c0		.
	rrca			;8804	0f		.
	rlca			;8805	07		.
	ret m			;8806	f8		.
	ld (hl),b		;8807	70		p
	rla			;8808	17		.
	ld (bc),a		;8809	02		.
	call m,00f38h		;880a	fc 38 0f	. 8 .
	rlca			;880d	07		.
	ret m			;880e	f8		.
	ret po			;880f	e0		.
	rra			;8810	1f		.
l8811h:
	ld c,0f0h		;8811	0e f0		. .
	ret nz			;8813	c0		.
	ld c,000h		;8814	0e 00		. .
	ret po			;8816	e0		.
	ld b,b			;8817	40		@
	ld bc,04000h		;8818	01 00 40	. . @
	nop			;881b	00		.
	ld (bc),a		;881c	02		.
	dec c			;881d	0d		.
	nop			;881e	00		.
	nop			;881f	00		.
	nop			;8820	00		.
	nop			;8821	00		.
	nop			;8822	00		.
	nop			;8823	00		.
	nop			;8824	00		.
	nop			;8825	00		.
	ld (bc),a		;8826	02		.
	nop			;8827	00		.
	ld b,b			;8828	40		@
	nop			;8829	00		.
	jr l882ch		;882a	18 00		. .
l882ch:
	ret pe			;882c	e8		.
	ld b,b			;882d	40		@
	ccf			;882e	3f		?
	jr l8811h		;882f	18 e0		. .
	ret nz			;8831	c0		.
	ccf			;8832	3f		?
	ld e,0fch		;8833	1e fc		. .
	ret po			;8835	e0		.
	rra			;8836	1f		.
	rrca			;8837	0f		.
	cp 0bch			;8838	fe bc		. .
	rrca			;883a	0f		.
	ld b,0fch		;883b	06 fc		. .
	jr c,l8866h		;883d	38 27		8 '
	inc bc			;883f	03		.
	ret m			;8840	f8		.
	ld (hl),b		;8841	70		p
	rrca			;8842	0f		.
	rlca			;8843	07		.
	call p,00fe0h		;8844	f4 e0 0f	. . .
	rlca			;8847	07		.
	ret m			;8848	f8		.
	ld (hl),b		;8849	70		p
	rra			;884a	1f		.
	inc c			;884b	0c		.
	ld a,b			;884c	78		x
	jr nc,l885bh		;884d	30 0c		0 .
	nop			;884f	00		.
	or b			;8850	b0		.
	nop			;8851	00		.
	ld (bc),a		;8852	02		.
	ld c,000h		;8853	0e 00		. .
	nop			;8855	00		.
	jr nz,l8858h		;8856	20 00		  .
l8858h:
	ex af,af'		;8858	08		.
	nop			;8859	00		.
	ld (hl),b		;885a	70		p
l885bh:
	jr nz,l885dh		;885b	20 00		  .
l885dh:
	nop			;885d	00		.
	call p,01f60h		;885e	f4 60 1f	. ` .
	nop			;8861	00		.
	ret p			;8862	f0		.
	ret po			;8863	e0		.
	ccf			;8864	3f		?
	rra			;8865	1f		.
l8866h:
	jp p,01fe0h		;8866	f2 e0 1f	. . .
	rrca			;8869	0f		.
	call m,00f30h		;886a	fc 30 0f	. 0 .
	ld b,0feh		;886d	06 fe		. .
	ld e,h			;886f	5c		\
	rla			;8870	17		.
	ld (bc),a		;8871	02		.
	call m,00f38h		;8872	fc 38 0f	. 8 .
	rlca			;8875	07		.
	ret m			;8876	f8		.
	and b			;8877	a0		.
	ld e,a			;8878	5f		_
	rrca			;8879	0f		.
	call p,01fe0h		;887a	f4 e0 1f	. . .
	ld c,0f8h		;887d	0e f8		. .
	ld (hl),b		;887f	70		p
	ld a,018h		;8880	3e 18		> .
	ld a,d			;8882	7a		z
	jr nc,l889eh		;8883	30 19		0 .
	nop			;8885	00		.
	jr c,$+18		;8886	38 10		8 .
	nop			;8888	00		.
	nop			;8889	00		.
	djnz l888ch		;888a	10 00		. .
l888ch:
	ld (bc),a		;888c	02		.
	djnz l888fh		;888d	10 00		. .
l888fh:
	nop			;888f	00		.
	add a,b			;8890	80		.
	nop			;8891	00		.
	ld c,c			;8892	49		I
	nop			;8893	00		.
	jp nc,00380h		;8894	d2 80 03	. . .
	ld bc,lc0e4h		;8897	01 e4 c0	. . .
	ld a,a			;889a	7f		.
	inc bc			;889b	03		.
	ret po			;889c	e0		.
	ret nz			;889d	c0		.
l889eh:
	rst 38h			;889e	ff		.
	ld a,e			;889f	7b		{
	cp 060h			;88a0	fe 60		. `
	ld a,a			;88a2	7f		.
	ld a,0ffh		;88a3	3e ff		> .
	ld a,(hl)		;88a5	7e		~
	ld a,a			;88a6	7f		.
l88a7h:
	jr c,l88a7h		;88a7	38 fe		8 .
	sbc a,h			;88a9	9c		.
	ccf			;88aa	3f		?
	dec d			;88ab	15		.
	call m,sub_bf38h	;88ac	fc 38 bf	. 8 .
	ld a,(de)		;88af	1a		.
	ret m			;88b0	f8		.
	ld (hl),b		;88b1	70		p
	rra			;88b2	1f		.
	inc c			;88b3	0c		.
	jp m,05fb0h		;88b4	fa b0 5f	. . _
	dec c			;88b7	0d		.
	call m,03fb8h		;88b8	fc b8 3f	. . ?
	rra			;88bb	1f		.
	call m,03ff8h		;88bc	fc f8 3f	. . ?
	ld e,0feh		;88bf	1e fe		. .
	call m,03c7eh		;88c1	fc 7e 3c	. ~ <
	cp 03ch			;88c4	fe 3c		. <
	ld a,l			;88c6	7d		}
	jr nc,$+64		;88c7	30 3e		0 >
	inc c			;88c9	0c		.
	jr nc,l88cch		;88ca	30 00		0 .
l88cch:
	inc c			;88cc	0c		.
	nop			;88cd	00		.
	ld (bc),a		;88ce	02		.
	inc de			;88cf	13		.
	ld b,b			;88d0	40		@
	nop			;88d1	00		.
	ld b,b			;88d2	40		@
	nop			;88d3	00		.
	nop			;88d4	00		.
	nop			;88d5	00		.
	call po,01140h		;88d6	e4 40 11	. @ .
	nop			;88d9	00		.
	pop hl			;88da	e1		.
	ret nz			;88db	c0		.
	ld b,e			;88dc	43		C
	ld bc,0e0f4h		;88dd	01 f4 e0	. . .
	inc de			;88e0	13		.
	ld bc,0e0f0h		;88e1	01 f0 e0	. . .
	ld a,a			;88e4	7f		.
	inc bc			;88e5	03		.
	ret m			;88e6	f8		.
	ld h,b			;88e7	60		`
	rst 38h			;88e8	ff		.
	ld a,a			;88e9	7f		.
	cp 078h			;88ea	fe 78		. x
	rst 38h			;88ec	ff		.
	ld a,(hl)		;88ed	7e		~
	rst 38h			;88ee	ff		.
	ld a,(hl)		;88ef	7e		~
	ld a,a			;88f0	7f		.
	ld (l8cfeh),a		;88f1	32 fe 8c	2 . .
	ld a,a			;88f4	7f		.
	add hl,sp		;88f5	39		9
	call m,03f58h		;88f6	fc 58 3f	. X ?
	ld e,0fdh		;88f9	1e fd		. .
	cp b			;88fb	b8		.
	sbc a,a			;88fc	9f		.
	dec c			;88fd	0d		.
	jp m,01f70h		;88fe	fa 70 1f	. p .
	ld a,(bc)		;8901	0a		.
	call m,03fb8h		;8902	fc b8 3f	. . ?
	dec de			;8905	1b		.
	call m,sub_7fd8h	;8906	fc d8 7f	. . .
	ccf			;8909	3f		?
	cp 07ch			;890a	fe 7c		. |
	ld a,a			;890c	7f		.
	ld a,07fh		;890d	3e 7f		> .
	ld a,0feh		;890f	3e fe		> .
	ld a,b			;8911	78		x
	ccf			;8912	3f		?
	ld c,0f9h		;8913	0e f9		. .
	ld h,b			;8915	60		`
	ld c,a			;8916	4f		O
	ld (bc),a		;8917	02		.
	ld h,b			;8918	60		`
	nop			;8919	00		.
l891ah:
	ld (bc),a		;891a	02		.
	nop			;891b	00		.
	inc bc			;891c	03		.
l891dh:
	dec de			;891d	1b		.
	inc c			;891e	0c		.
	nop			;891f	00		.
	ld bc,l8000h		;8920	01 00 80	. . .
	nop			;8923	00		.
	ld e,004h		;8924	1e 04		. .
	ei			;8926	fb		.
	nop			;8927	00		.
	ret nz			;8928	c0		.
	add a,b			;8929	80		.
	ccf			;892a	3f		?
	ld d,0ffh		;892b	16 ff		. .
	jp m,lc0e0h		;892d	fa e0 c0	. . .
	ld a,a			;8930	7f		.
	daa			;8931	27		'
	rst 38h			;8932	ff		.
	ld (hl),h		;8933	74		t
	ret p			;8934	f0		.
	ret po			;8935	e0		.
	ld a,a			;8936	7f		.
	daa			;8937	27		'
	rst 38h			;8938	ff		.
	inc b			;8939	04		.
	ret p			;893a	f0		.
	ret po			;893b	e0		.
	rst 38h			;893c	ff		.
	ld b,a			;893d	47		G
	rst 18h			;893e	df		.
	adc a,b			;893f	88		.
	ret m			;8940	f8		.
	ret p			;8941	f0		.
	rst 38h			;8942	ff		.
	ld c,a			;8943	4f		O
	rst 38h			;8944	ff		.
	adc a,c			;8945	89		.
	ret m			;8946	f8		.
	ret p			;8947	f0		.
	rst 38h			;8948	ff		.
	ld c,a			;8949	4f		O
	rst 38h			;894a	ff		.
	xor c			;894b	a9		.
	ret m			;894c	f8		.
	ret p			;894d	f0		.
	ld a,a			;894e	7f		.
	jr nc,$+1		;894f	30 ff		0 .
	ld h,0f0h		;8951	26 f0		& .
	nop			;8953	00		.
	rst 38h			;8954	ff		.
	ld c,a			;8955	4f		O
	rst 38h			;8956	ff		.
	adc a,c			;8957	89		.
	ret m			;8958	f8		.
	ret p			;8959	f0		.
	rst 38h			;895a	ff		.
	ld c,a			;895b	4f		O
	rst 18h			;895c	df		.
	adc a,c			;895d	89		.
	ret m			;895e	f8		.
	ret p			;895f	f0		.
	ld a,a			;8960	7f		.
	nop			;8961	00		.
	adc a,a			;8962	8f		.
	nop			;8963	00		.
	ret p			;8964	f0		.
	nop			;8965	00		.
	inc e			;8966	1c		.
	nop			;8967	00		.
	ld bc,l8000h		;8968	01 00 80	. . .
	nop			;896b	00		.
	ld a,01ch		;896c	3e 1c		> .
	inc bc			;896e	03		.
	ld bc,l80c0h		;896f	01 c0 80	. . .
	ld a,a			;8972	7f		.
	ld e,003h		;8973	1e 03		. .
	ld bc,040e0h		;8975	01 e0 40	. . @
	ld a,a			;8978	7f		.
	ld (hl),007h		;8979	36 07		6 .
	ld (bc),a		;897b	02		.
	ret po			;897c	e0		.
	add a,b			;897d	80		.
	ld a,a			;897e	7f		.
	dec hl			;897f	2b		+
	add a,a			;8980	87		.
	nop			;8981	00		.
	ret p			;8982	f0		.
	jr nz,$+1		;8983	20 ff		  .
	ld d,e			;8985	53		S
	add a,a			;8986	87		.
	ld (bc),a		;8987	02		.
	ret po			;8988	e0		.
	add a,b			;8989	80		.
	ld a,a			;898a	7f		.
	add hl,bc		;898b	09		.
	add a,e			;898c	83		.
	nop			;898d	00		.
	ret po			;898e	e0		.
	ld b,b			;898f	40		@
	ld a,a			;8990	7f		.
	ld (00183h),hl		;8991	22 83 01	" . .
	ret nz			;8994	c0		.
	nop			;8995	00		.
	ld a,a			;8996	7f		.
	djnz l891ah		;8997	10 81		. .
	nop			;8999	00		.
	ret nz			;899a	c0		.
	add a,b			;899b	80		.
	ld a,a			;899c	7f		.
	add hl,hl		;899d	29		)
	add a,c			;899e	81		.
	nop			;899f	00		.
	ret nz			;89a0	c0		.
	nop			;89a1	00		.
	ccf			;89a2	3f		?
	nop			;89a3	00		.
	add a,c			;89a4	81		.
	nop			;89a5	00		.
	ret nz			;89a6	c0		.
	add a,b			;89a7	80		.
	ccf			;89a8	3f		?
	dec d			;89a9	15		.
	add a,b			;89aa	80		.
	nop			;89ab	00		.
	add a,b			;89ac	80		.
	nop			;89ad	00		.
	rra			;89ae	1f		.
	nop			;89af	00		.
	nop			;89b0	00		.
	nop			;89b1	00		.
	nop			;89b2	00		.
	nop			;89b3	00		.
	rra			;89b4	1f		.
	inc b			;89b5	04		.
	nop			;89b6	00		.
	nop			;89b7	00		.
	nop			;89b8	00		.
	nop			;89b9	00		.
	ld c,000h		;89ba	0e 00		. .
	nop			;89bc	00		.
	nop			;89bd	00		.
	nop			;89be	00		.
	nop			;89bf	00		.
	inc bc			;89c0	03		.
	inc e			;89c1	1c		.
	inc c			;89c2	0c		.
	nop			;89c3	00		.
	ld bc,l8000h		;89c4	01 00 80	. . .
	nop			;89c7	00		.
	ld e,004h		;89c8	1e 04		. .
	ei			;89ca	fb		.
	nop			;89cb	00		.
	ret nz			;89cc	c0		.
	add a,b			;89cd	80		.
	ccf			;89ce	3f		?
	ld d,0ffh		;89cf	16 ff		. .
	jp m,lc0e0h		;89d1	fa e0 c0	. . .
	ld a,a			;89d4	7f		.
	daa			;89d5	27		'
	rst 38h			;89d6	ff		.
	ld (hl),h		;89d7	74		t
	ret p			;89d8	f0		.
	ret po			;89d9	e0		.
	ld a,a			;89da	7f		.
	daa			;89db	27		'
	rst 38h			;89dc	ff		.
	inc b			;89dd	04		.
	ret p			;89de	f0		.
	ret po			;89df	e0		.
	rst 38h			;89e0	ff		.
	ld b,a			;89e1	47		G
	rst 18h			;89e2	df		.
	adc a,b			;89e3	88		.
	ret m			;89e4	f8		.
	ret p			;89e5	f0		.
	rst 38h			;89e6	ff		.
	ld c,a			;89e7	4f		O
	rst 38h			;89e8	ff		.
	adc a,c			;89e9	89		.
	ret m			;89ea	f8		.
	ret p			;89eb	f0		.
	rst 38h			;89ec	ff		.
	ld c,a			;89ed	4f		O
	rst 38h			;89ee	ff		.
	xor c			;89ef	a9		.
	ret m			;89f0	f8		.
	ret p			;89f1	f0		.
	ld a,a			;89f2	7f		.
	jr nc,$+1		;89f3	30 ff		0 .
	ld h,0f0h		;89f5	26 f0		& .
	nop			;89f7	00		.
	rst 38h			;89f8	ff		.
	ld c,a			;89f9	4f		O
	rst 38h			;89fa	ff		.
	adc a,c			;89fb	89		.
	ret m			;89fc	f8		.
	ret p			;89fd	f0		.
	rst 38h			;89fe	ff		.
	ld c,a			;89ff	4f		O
	rst 18h			;8a00	df		.
	adc a,c			;8a01	89		.
	ret m			;8a02	f8		.
	ret p			;8a03	f0		.
	ld a,a			;8a04	7f		.
	nop			;8a05	00		.
	adc a,a			;8a06	8f		.
	nop			;8a07	00		.
	ret p			;8a08	f0		.
	nop			;8a09	00		.
	inc c			;8a0a	0c		.
	nop			;8a0b	00		.
	ld bc,lc000h		;8a0c	01 00 c0	. . .
	nop			;8a0f	00		.
	ld e,00ch		;8a10	1e 0c		. .
	inc bc			;8a12	03		.
	ld bc,lc0e0h		;8a13	01 e0 c0	. . .
	ld a,014h		;8a16	3e 14		> .
	rlca			;8a18	07		.
	ld (bc),a		;8a19	02		.
	ret p			;8a1a	f0		.
	and b			;8a1b	a0		.
	ccf			;8a1c	3f		?
	ld a,(bc)		;8a1d	0a		.
	rlca			;8a1e	07		.
	ld bc,l70f8h		;8a1f	01 f8 70	. . p
	ld a,a			;8a22	7f		.
	jr nz,l8a34h		;8a23	20 0f		  .
	ld b,0f8h		;8a25	06 f8		. .
	and b			;8a27	a0		.
	ld a,a			;8a28	7f		.
	ld a,(bc)		;8a29	0a		.
	rrca			;8a2a	0f		.
	ld (bc),a		;8a2b	02		.
	ret m			;8a2c	f8		.
	add a,b			;8a2d	80		.
	ld a,010h		;8a2e	3e 10		> .
	rrca			;8a30	0f		.
	inc b			;8a31	04		.
	ret m			;8a32	f8		.
	ld d,b			;8a33	50		P
l8a34h:
	ld a,004h		;8a34	3e 04		> .
	rrca			;8a36	0f		.
	ld bc,000f0h		;8a37	01 f0 00	. . .
	inc e			;8a3a	1c		.
	ex af,af'		;8a3b	08		.
	rrca			;8a3c	0f		.
	inc b			;8a3d	04		.
	ret p			;8a3e	f0		.
	ld b,b			;8a3f	40		@
	inc e			;8a40	1c		.
	nop			;8a41	00		.
	rrca			;8a42	0f		.
	nop			;8a43	00		.
	ret p			;8a44	f0		.
	and b			;8a45	a0		.
	inc e			;8a46	1c		.
	ex af,af'		;8a47	08		.
	rlca			;8a48	07		.
	ld bc,000e0h		;8a49	01 e0 00	. . .
	ex af,af'		;8a4c	08		.
	nop			;8a4d	00		.
	rlca			;8a4e	07		.
	nop			;8a4f	00		.
	ret po			;8a50	e0		.
	ld b,b			;8a51	40		@
	nop			;8a52	00		.
	nop			;8a53	00		.
	inc bc			;8a54	03		.
	ld bc,000e0h		;8a55	01 e0 00	. . .
	nop			;8a58	00		.
	nop			;8a59	00		.
	inc bc			;8a5a	03		.
	nop			;8a5b	00		.
	ret nz			;8a5c	c0		.
	nop			;8a5d	00		.
	nop			;8a5e	00		.
	nop			;8a5f	00		.
	ld bc,lc000h		;8a60	01 00 c0	. . .
	add a,b			;8a63	80		.
	nop			;8a64	00		.
	nop			;8a65	00		.
	nop			;8a66	00		.
	nop			;8a67	00		.
	add a,b			;8a68	80		.
	nop			;8a69	00		.
	inc bc			;8a6a	03		.
	rrca			;8a6b	0f		.
	nop			;8a6c	00		.
	nop			;8a6d	00		.
	nop			;8a6e	00		.
	nop			;8a6f	00		.
	ld (hl),b		;8a70	70		p
	nop			;8a71	00		.
	jr c,l8a74h		;8a72	38 00		8 .
l8a74h:
	jr nz,l8a76h		;8a74	20 00		  .
l8a76h:
	ret m			;8a76	f8		.
	ld (hl),b		;8a77	70		p
	ld a,h			;8a78	7c		|
l8a79h:
	jr c,l8aech		;8a79	38 71		8 q
	jr nz,l8a79h		;8a7b	20 fc		  .
	sbc a,b			;8a7d	98		.
	rst 38h			;8a7e	ff		.
	ld c,h			;8a7f	4c		L
	ei			;8a80	fb		.
	ld sp,03cfeh		;8a81	31 fe 3c	1 . <
	rst 38h			;8a84	ff		.
	ld e,l			;8a85	5d		]
	rst 38h			;8a86	ff		.
	ld sp,hl		;8a87	f9		.
	cp 07ch			;8a88	fe 7c		. |
	rst 38h			;8a8a	ff		.
	ld a,h			;8a8b	7c		|
	ei			;8a8c	fb		.
	ld sp,0fcfeh		;8a8d	31 fe fc	1 . .
	ld a,h			;8a90	7c		|
l8a91h:
	jr c,l8b04h		;8a91	38 71		8 q
	jr nz,l8a91h		;8a93	20 fc		  .
	ret m			;8a95	f8		.
	jr c,l8a98h		;8a96	38 00		8 .
l8a98h:
	jr nz,l8a9ah		;8a98	20 00		  .
l8a9ah:
	ret m			;8a9a	f8		.
	ld (hl),b		;8a9b	70		p
	ld a,a			;8a9c	7f		.
	nop			;8a9d	00		.
	defb 0ddh,000h,0fch ;illegal sequence	;8a9e	dd 00 fc	. . .
	nop			;8aa1	00		.
	rst 38h			;8aa2	ff		.
	ld (hl),a		;8aa3	77		w
	rst 38h			;8aa4	ff		.
	defb 0ddh,0feh,0d4h ;illegal sequence	;8aa5	dd fe d4	. . .
	rst 38h			;8aa8	ff		.
	ld b,l			;8aa9	45		E
	rst 38h			;8aaa	ff		.
	ld d,l			;8aab	55		U
	cp 014h			;8aac	fe 14		. .
	rst 38h			;8aae	ff		.
	ld (hl),l		;8aaf	75		u
	rst 38h			;8ab0	ff		.
	ld e,l			;8ab1	5d		]
	cp 0dch			;8ab2	fe dc		. .
	ld a,a			;8ab4	7f		.
	dec d			;8ab5	15		.
	rst 38h			;8ab6	ff		.
	ld d,h			;8ab7	54		T
	cp 054h			;8ab8	fe 54		. T
	rst 38h			;8aba	ff		.
	ld (hl),h		;8abb	74		t
	rst 38h			;8abc	ff		.
	ld d,l			;8abd	55		U
	cp 0d4h			;8abe	fe d4		. .
	ld (hl),h		;8ac0	74		t
	nop			;8ac1	00		.
	ld d,l			;8ac2	55		U
	nop			;8ac3	00		.
	call nc,00300h		;8ac4	d4 00 03	. . .
	rrca			;8ac7	0f		.
	rrca			;8ac8	0f		.
	nop			;8ac9	00		.
	rst 38h			;8aca	ff		.
	nop			;8acb	00		.
	ret p			;8acc	f0		.
	nop			;8acd	00		.
	rra			;8ace	1f		.
	rrca			;8acf	0f		.
	rst 38h			;8ad0	ff		.
	rst 38h			;8ad1	ff		.
	ret m			;8ad2	f8		.
	ret p			;8ad3	f0		.
	rra			;8ad4	1f		.
	dec c			;8ad5	0d		.
	rst 38h			;8ad6	ff		.
	xor e			;8ad7	ab		.
	ret m			;8ad8	f8		.
	ld (hl),b		;8ad9	70		p
	ccf			;8ada	3f		?
	dec e			;8adb	1d		.
	rst 38h			;8adc	ff		.
	ld l,e			;8add	6b		k
	call m,03f78h		;8ade	fc 78 3f	. x ?
	inc e			;8ae1	1c		.
	rst 38h			;8ae2	ff		.
	ex de,hl		;8ae3	eb		.
	call m,sub_7f78h	;8ae4	fc 78 7f	. x .
	dec a			;8ae7	3d		=
	rst 38h			;8ae8	ff		.
	ld l,e			;8ae9	6b		k
	cp 07ch			;8aea	fe 7c		. |
l8aech:
	ld a,a			;8aec	7f		.
	dec a			;8aed	3d		=
	rst 38h			;8aee	ff		.
	xor c			;8aef	a9		.
	cp 03ch			;8af0	fe 3c		. <
	rst 38h			;8af2	ff		.
	ld a,a			;8af3	7f		.
	rst 38h			;8af4	ff		.
	rst 38h			;8af5	ff		.
	rst 38h			;8af6	ff		.
	cp 0ffh			;8af7	fe ff		. .
	ld b,l			;8af9	45		E
	rst 38h			;8afa	ff		.
	and d			;8afb	a2		.
	rst 38h			;8afc	ff		.
	ld (055ffh),hl		;8afd	22 ff 55	" . U
	rst 38h			;8b00	ff		.
	xor (hl)		;8b01	ae		.
	rst 38h			;8b02	ff		.
	xor (hl)		;8b03	ae		.
l8b04h:
	rst 38h			;8b04	ff		.
	ld b,l			;8b05	45		E
	rst 38h			;8b06	ff		.
	and d			;8b07	a2		.
	rst 38h			;8b08	ff		.
	and d			;8b09	a2		.
	rst 38h			;8b0a	ff		.
	ld d,l			;8b0b	55		U
	rst 38h			;8b0c	ff		.
	xor (hl)		;8b0d	ae		.
	rst 38h			;8b0e	ff		.
	cp d			;8b0f	ba		.
	rst 38h			;8b10	ff		.
	ld d,h			;8b11	54		T
	rst 38h			;8b12	ff		.
	and d			;8b13	a2		.
	rst 38h			;8b14	ff		.
	and d			;8b15	a2		.
	rst 38h			;8b16	ff		.
	ld a,a			;8b17	7f		.
	rst 38h			;8b18	ff		.
	rst 38h			;8b19	ff		.
	rst 38h			;8b1a	ff		.
	cp 07fh			;8b1b	fe 7f		. .
	nop			;8b1d	00		.
	rst 38h			;8b1e	ff		.
	nop			;8b1f	00		.
	cp 000h			;8b20	fe 00		. .
	inc bc			;8b22	03		.
	inc c			;8b23	0c		.
	nop			;8b24	00		.
	nop			;8b25	00		.
	nop			;8b26	00		.
	nop			;8b27	00		.
	ld h,b			;8b28	60		`
	nop			;8b29	00		.
	nop			;8b2a	00		.
	nop			;8b2b	00		.
	jr nc,l8b2eh		;8b2c	30 00		0 .
l8b2eh:
	ret p			;8b2e	f0		.
	ld h,b			;8b2f	60		`
	nop			;8b30	00		.
	nop			;8b31	00		.
	ld sp,hl		;8b32	f9		.
	jr nc,$-6		;8b33	30 f8		0 .
	ld (hl),b		;8b35	70		p
	inc bc			;8b36	03		.
	nop			;8b37	00		.
	rst 38h			;8b38	ff		.
	pop af			;8b39	f1		.
	call m,007b8h		;8b3a	fc b8 07	. . .
	inc bc			;8b3d	03		.
	rst 38h			;8b3e	ff		.
	and 0fch		;8b3f	e6 fc		. .
	ret c			;8b41	d8		.
	ld a,a			;8b42	7f		.
	rlca			;8b43	07		.
	rst 38h			;8b44	ff		.
	add a,e			;8b45	83		.
	call m,0ff58h		;8b46	fc 58 ff	. X .
	ld l,a			;8b49	6f		o
	rst 38h			;8b4a	ff		.
	call m,038fch		;8b4b	fc fc 38	. . 8
	rst 38h			;8b4e	ff		.
	rrca			;8b4f	0f		.
	rst 38h			;8b50	ff		.
	rst 38h			;8b51	ff		.
	ret m			;8b52	f8		.
	ret p			;8b53	f0		.
	rst 38h			;8b54	ff		.
	ld l,a			;8b55	6f		o
	rst 38h			;8b56	ff		.
	rst 38h			;8b57	ff		.
	ret p			;8b58	f0		.
	ret po			;8b59	e0		.
	rst 38h			;8b5a	ff		.
	ld l,a			;8b5b	6f		o
	rst 38h			;8b5c	ff		.
	rst 38h			;8b5d	ff		.
	ret po			;8b5e	e0		.
	ret nz			;8b5f	c0		.
	rst 38h			;8b60	ff		.
	ld l,a			;8b61	6f		o
	rst 38h			;8b62	ff		.
	cp 0c0h			;8b63	fe c0		. .
	nop			;8b65	00		.
	ld l,a			;8b66	6f		o
	nop			;8b67	00		.
	cp 000h			;8b68	fe 00		. .
	nop			;8b6a	00		.
	nop			;8b6b	00		.
	inc bc			;8b6c	03		.
	dec bc			;8b6d	0b		.
	ccf			;8b6e	3f		?
	nop			;8b6f	00		.
	rst 38h			;8b70	ff		.
	nop			;8b71	00		.
	ret p			;8b72	f0		.
	nop			;8b73	00		.
	ld a,a			;8b74	7f		.
	scf			;8b75	37		7
	rst 38h			;8b76	ff		.
	rst 38h			;8b77	ff		.
	ret m			;8b78	f8		.
	or b			;8b79	b0		.
	rst 38h			;8b7a	ff		.
	ld c,b			;8b7b	48		H
	rst 38h			;8b7c	ff		.
	nop			;8b7d	00		.
	call m,0ff48h		;8b7e	fc 48 ff	. H .
	ld c,b			;8b81	48		H
	rst 38h			;8b82	ff		.
	nop			;8b83	00		.
	call m,sub_7f48h	;8b84	fc 48 7f	. H .
	rla			;8b87	17		.
	rst 38h			;8b88	ff		.
	rst 38h			;8b89	ff		.
	ret m			;8b8a	f8		.
	and b			;8b8b	a0		.
	ld a,a			;8b8c	7f		.
	jr nc,$+1		;8b8d	30 ff		0 .
	call m,030f8h		;8b8f	fc f8 30	. . 0
	rst 38h			;8b92	ff		.
	ld a,a			;8b93	7f		.
	rst 38h			;8b94	ff		.
	ld a,e			;8b95	7b		{
	call m,sub_7ff8h	;8b96	fc f8 7f	. . .
	jr nc,$+1		;8b99	30 ff		0 .
	call m,030f8h		;8b9b	fc f8 30	. . 0
	ccf			;8b9e	3f		?
	rla			;8b9f	17		.
	rst 38h			;8ba0	ff		.
	rst 38h			;8ba1	ff		.
	ret p			;8ba2	f0		.
l8ba3h:
	and b			;8ba3	a0		.
	rra			;8ba4	1f		.
	rrca			;8ba5	0f		.
	rst 38h			;8ba6	ff		.
	rst 38h			;8ba7	ff		.
	ret po			;8ba8	e0		.
	ret nz			;8ba9	c0		.
	rrca			;8baa	0f		.
	nop			;8bab	00		.
	rst 38h			;8bac	ff		.
	nop			;8bad	00		.
	ret nz			;8bae	c0		.
	nop			;8baf	00		.
	inc bc			;8bb0	03		.
	rrca			;8bb1	0f		.
	inc e			;8bb2	1c		.
	nop			;8bb3	00		.
	nop			;8bb4	00		.
	nop			;8bb5	00		.
	nop			;8bb6	00		.
	nop			;8bb7	00		.
	ld a,01ch		;8bb8	3e 1c		> .
	nop			;8bba	00		.
	nop			;8bbb	00		.
	nop			;8bbc	00		.
l8bbdh:
	nop			;8bbd	00		.
	ld a,a			;8bbe	7f		.
	ld (00008h),hl		;8bbf	22 08 00	" . .
	nop			;8bc2	00		.
	nop			;8bc3	00		.
	rst 38h			;8bc4	ff		.
	ld c,a			;8bc5	4f		O
	sbc a,h			;8bc6	9c		.
	ex af,af'		;8bc7	08		.
	nop			;8bc8	00		.
	nop			;8bc9	00		.
	rst 38h			;8bca	ff		.
	ld e,a			;8bcb	5f		_
	sbc a,h			;8bcc	9c		.
	ex af,af'		;8bcd	08		.
	add a,b			;8bce	80		.
	nop			;8bcf	00		.
	rst 38h			;8bd0	ff		.
	ld e,a			;8bd1	5f		_
	sbc a,l			;8bd2	9d		.
	ex af,af'		;8bd3	08		.
	ret nz			;8bd4	c0		.
	add a,b			;8bd5	80		.
	ld a,a			;8bd6	7f		.
	ld a,039h		;8bd7	3e 39		> 9
	djnz l8ba3h		;8bd9	10 c8		. .
	add a,b			;8bdb	80		.
	ld a,01ch		;8bdc	3e 1c		> .
	add hl,sp		;8bde	39		9
	djnz l8bbdh		;8bdf	10 dc		. .
	adc a,b			;8be1	88		.
	inc a			;8be2	3c		<
	nop			;8be3	00		.
	inc de			;8be4	13		.
	ld bc,0089ch		;8be5	01 9c 08	. . .
	ld a,a			;8be8	7f		.
	inc a			;8be9	3c		<
	ld bc,03800h		;8bea	01 00 38	. . 8
	djnz l8c6eh		;8bed	10 7f		. .
	ld hl,0009ch		;8bef	21 9c 00	! . .
	djnz l8bf4h		;8bf2	10 00		. .
l8bf4h:
	ld a,a			;8bf4	7f		.
	dec a			;8bf5	3d		=
	cp a			;8bf6	bf		.
	inc e			;8bf7	1c		.
	ret p			;8bf8	f0		.
	nop			;8bf9	00		.
	ccf			;8bfa	3f		?
	dec b			;8bfb	05		.
	rst 38h			;8bfc	ff		.
	dec d			;8bfd	15		.
	ret m			;8bfe	f8		.
	ld d,b			;8bff	50		P
	ld a,a			;8c00	7f		.
	dec a			;8c01	3d		=
	rst 38h			;8c02	ff		.
	defb 0ddh,0f8h,0f0h ;illegal sequence	;8c03	dd f8 f0	. . .
	ccf			;8c06	3f		?
	nop			;8c07	00		.
	rst 38h			;8c08	ff		.
	nop			;8c09	00		.
	ret p			;8c0a	f0		.
	nop			;8c0b	00		.
	inc bc			;8c0c	03		.
	add hl,bc		;8c0d	09		.
	ccf			;8c0e	3f		?
l8c0fh:
	nop			;8c0f	00		.
	rst 38h			;8c10	ff		.
	nop			;8c11	00		.
	ret nz			;8c12	c0		.
	nop			;8c13	00		.
	ld a,a			;8c14	7f		.
	ld a,0ffh		;8c15	3e ff		> .
	or a			;8c17	b7		.
	ret po			;8c18	e0		.
	ret nz			;8c19	c0		.
	rst 38h			;8c1a	ff		.
	ld b,c			;8c1b	41		A
	rst 38h			;8c1c	ff		.
	jr c,l8c0fh		;8c1d	38 f0		8 .
	jr nz,$+1		;8c1f	20 ff		  .
	ld b,d			;8c21	42		B
	rst 38h			;8c22	ff		.
	ld a,h			;8c23	7c		|
sub_8c24h:
	ret p			;8c24	f0		.
	jr nz,$+129		;8c25	20 7f		  .
	inc a			;8c27	3c		<
	rst 38h			;8c28	ff		.
	inc bc			;8c29	03		.
	ret po			;8c2a	e0		.
	ret nz			;8c2b	c0		.
	ld a,a			;8c2c	7f		.
	ccf			;8c2d	3f		?
	rst 38h			;8c2e	ff		.
	rst 38h			;8c2f	ff		.
	ret po			;8c30	e0		.
	ret nz			;8c31	c0		.
	ccf			;8c32	3f		?
	rra			;8c33	1f		.
	rst 38h			;8c34	ff		.
	rst 38h			;8c35	ff		.
	ret nz			;8c36	c0		.
	add a,b			;8c37	80		.
	rra			;8c38	1f		.
	rrca			;8c39	0f		.
	rst 38h			;8c3a	ff		.
	rst 38h			;8c3b	ff		.
	add a,b			;8c3c	80		.
	nop			;8c3d	00		.
	rrca			;8c3e	0f		.
	nop			;8c3f	00		.
	rst 38h			;8c40	ff		.
	nop			;8c41	00		.
	nop			;8c42	00		.
	nop			;8c43	00		.
	inc bc			;8c44	03		.
	dec c			;8c45	0d		.
	ccf			;8c46	3f		?
	nop			;8c47	00		.
	rst 38h			;8c48	ff		.
	nop			;8c49	00		.
	add a,b			;8c4a	80		.
	nop			;8c4b	00		.
	ld a,a			;8c4c	7f		.
	scf			;8c4d	37		7
	rst 38h			;8c4e	ff		.
	defb 0fdh,0c0h,080h ;illegal sequence	;8c4f	fd c0 80	. . .
	rst 38h			;8c52	ff		.
	ld b,b			;8c53	40		@
	rst 38h			;8c54	ff		.
	ld bc,lc0e0h		;8c55	01 e0 c0	. . .
	rst 38h			;8c58	ff		.
	scf			;8c59	37		7
	rst 38h			;8c5a	ff		.
	defb 0fdh,0e0h,0c0h ;illegal sequence	;8c5b	fd e0 c0	. . .
	rst 38h			;8c5e	ff		.
	ld (hl),a		;8c5f	77		w
	rst 38h			;8c60	ff		.
	defb 0fdh,0e0h,0c0h ;illegal sequence	;8c61	fd e0 c0	. . .
	ld a,a			;8c64	7f		.
	scf			;8c65	37		7
	rst 38h			;8c66	ff		.
	defb 0fdh,0c0h,080h ;illegal sequence	;8c67	fd c0 80	. . .
	ccf			;8c6a	3f		?
	nop			;8c6b	00		.
	rst 38h			;8c6c	ff		.
	nop			;8c6d	00		.
l8c6eh:
	add a,b			;8c6e	80		.
	nop			;8c6f	00		.
	ld (bc),a		;8c70	02		.
	nop			;8c71	00		.
	inc e			;8c72	1c		.
	ex af,af'		;8c73	08		.
	nop			;8c74	00		.
	nop			;8c75	00		.
	rlca			;8c76	07		.
	ld (bc),a		;8c77	02		.
	ld a,018h		;8c78	3e 18		> .
	nop			;8c7a	00		.
	nop			;8c7b	00		.
	rrca			;8c7c	0f		.
	rlca			;8c7d	07		.
	sbc a,h			;8c7e	9c		.
	ex af,af'		;8c7f	08		.
	nop			;8c80	00		.
	nop			;8c81	00		.
	rlca			;8c82	07		.
	ld (bc),a		;8c83	02		.
	inc e			;8c84	1c		.
	ex af,af'		;8c85	08		.
	nop			;8c86	00		.
	nop			;8c87	00		.
	ld (bc),a		;8c88	02		.
	nop			;8c89	00		.
	ld a,01ch		;8c8a	3e 1c		> .
	nop			;8c8c	00		.
	nop			;8c8d	00		.
	nop			;8c8e	00		.
	nop			;8c8f	00		.
	inc e			;8c90	1c		.
	nop			;8c91	00		.
	nop			;8c92	00		.
	nop			;8c93	00		.
	inc bc			;8c94	03		.
	ld c,07fh		;8c95	0e 7f		. .
	nop			;8c97	00		.
	rst 38h			;8c98	ff		.
	nop			;8c99	00		.
	call m,0ff00h		;8c9a	fc 00 ff	. . .
	ld a,a			;8c9d	7f		.
	rst 38h			;8c9e	ff		.
	rst 38h			;8c9f	ff		.
	cp 0fch			;8ca0	fe fc		. .
	rst 38h			;8ca2	ff		.
	ld b,d			;8ca3	42		B
	rst 38h			;8ca4	ff		.
l8ca5h:
	djnz l8ca5h		;8ca5	10 fe		. .
	add a,h			;8ca7	84		.
	rst 38h			;8ca8	ff		.
	ld e,(hl)		;8ca9	5e		^
	rst 38h			;8caa	ff		.
	sub 0feh		;8cab	d6 fe		. .
	or h			;8cad	b4		.
	rst 38h			;8cae	ff		.
	ld b,d			;8caf	42		B
	rst 38h			;8cb0	ff		.
	sub 0feh		;8cb1	d6 fe		. .
	or h			;8cb3	b4		.
	rst 38h			;8cb4	ff		.
	ld a,d			;8cb5	7a		z
	rst 38h			;8cb6	ff		.
	sub 0feh		;8cb7	d6 fe		. .
	or h			;8cb9	b4		.
	rst 38h			;8cba	ff		.
	ld b,d			;8cbb	42		B
	rst 38h			;8cbc	ff		.
l8cbdh:
	djnz l8cbdh		;8cbd	10 fe		. .
	add a,h			;8cbf	84		.
l8cc0h:
	rst 38h			;8cc0	ff		.
	ld a,a			;8cc1	7f		.
	rst 38h			;8cc2	ff		.
	rst 38h			;8cc3	ff		.
	cp 0fch			;8cc4	fe fc		. .
	ld a,a			;8cc6	7f		.
	rra			;8cc7	1f		.
	rst 38h			;8cc8	ff		.
	rst 38h			;8cc9	ff		.
	call m,01ff0h		;8cca	fc f0 1f	. . .
	rlca			;8ccd	07		.
	rst 38h			;8cce	ff		.
	rst 38h			;8ccf	ff		.
	ret p			;8cd0	f0		.
	ret nz			;8cd1	c0		.
	rlca			;8cd2	07		.
	ld bc,0ffffh		;8cd3	01 ff ff	. . .
	ret nz			;8cd6	c0		.
	nop			;8cd7	00		.
	ld bc,0ff00h		;8cd8	01 00 ff	. . .
	ld a,h			;8cdb	7c		|
	nop			;8cdc	00		.
	nop			;8cdd	00		.
	nop			;8cde	00		.
	nop			;8cdf	00		.
	ld a,h			;8ce0	7c		|
	djnz l8ce3h		;8ce1	10 00		. .
l8ce3h:
	nop			;8ce3	00		.
	nop			;8ce4	00		.
	nop			;8ce5	00		.
	djnz l8ce8h		;8ce6	10 00		. .
l8ce8h:
	nop			;8ce8	00		.
	nop			;8ce9	00		.
	inc bc			;8cea	03		.
	rrca			;8ceb	0f		.
	nop			;8cec	00		.
	nop			;8ced	00		.
	jr nz,l8cf0h		;8cee	20 00		  .
l8cf0h:
	nop			;8cf0	00		.
	nop			;8cf1	00		.
	nop			;8cf2	00		.
	nop			;8cf3	00		.
	ld (hl),b		;8cf4	70		p
	jr nz,l8cf7h		;8cf5	20 00		  .
l8cf7h:
	nop			;8cf7	00		.
	rra			;8cf8	1f		.
	nop			;8cf9	00		.
	rst 38h			;8cfa	ff		.
	ld (hl),b		;8cfb	70		p
	ret nz			;8cfc	c0		.
	nop			;8cfd	00		.
l8cfeh:
	ccf			;8cfe	3f		?
	ld e,0ffh		;8cff	1e ff		. .
	ei			;8d01	fb		.
	ret po			;8d02	e0		.
	ret nz			;8d03	c0		.
	ccf			;8d04	3f		?
	inc e			;8d05	1c		.
	rst 38h			;8d06	ff		.
	ld hl,lc0e0h		;8d07	21 e0 c0	! . .
	ccf			;8d0a	3f		?
	inc e			;8d0b	1c		.
	rst 38h			;8d0c	ff		.
	ld hl,lc0e0h		;8d0d	21 e0 c0	! . .
	ccf			;8d10	3f		?
	ld (de),a		;8d11	12		.
	rst 38h			;8d12	ff		.
	ld (040e0h),hl		;8d13	22 e0 40	" . @
	dec de			;8d16	1b		.
	ld bc,004feh		;8d17	01 fe 04	. . .
	ld b,b			;8d1a	40		@
	nop			;8d1b	00		.
	ld bc,0fc00h		;8d1c	01 00 fc	. . .
	nop			;8d1f	00		.
	nop			;8d20	00		.
	nop			;8d21	00		.
	nop			;8d22	00		.
	nop			;8d23	00		.
	ret m			;8d24	f8		.
	ld (hl),b		;8d25	70		p
	nop			;8d26	00		.
	nop			;8d27	00		.
	ld bc,0fc00h		;8d28	01 00 fc	. . .
	sbc a,b			;8d2b	98		.
	nop			;8d2c	00		.
	nop			;8d2d	00		.
	ld bc,0fc00h		;8d2e	01 00 fc	. . .
	cp b			;8d31	b8		.
	nop			;8d32	00		.
	nop			;8d33	00		.
	ld bc,0fc00h		;8d34	01 00 fc	. . .
	cp b			;8d37	b8		.
	nop			;8d38	00		.
	nop			;8d39	00		.
	nop			;8d3a	00		.
	nop			;8d3b	00		.
	ret m			;8d3c	f8		.
	ld (hl),b		;8d3d	70		p
	nop			;8d3e	00		.
	nop			;8d3f	00		.
	nop			;8d40	00		.
	nop			;8d41	00		.
	ld (hl),b		;8d42	70		p
	nop			;8d43	00		.
	nop			;8d44	00		.
	nop			;8d45	00		.
l8d46h:
	nop			;8d46	00		.
	nop			;8d47	00		.
l8d48h:
	rla			;8d48	17		.
l8d49h:
	adc a,(hl)		;8d49	8e		.
l8d4ah:
	nop			;8d4a	00		.
	add a,b			;8d4b	80		.
sub_8d4ch:
	ld a,01eh		;8d4c	3e 1e		> .
	ld (l7939h),a		;8d4e	32 39 79	2 9 y
	ld a,(lb7eah)		;8d51	3a ea b7	: . .
	add a,a			;8d54	87		.
	ld hl,l8e06h		;8d55	21 06 8e	! . .
	call sub_b5bbh		;8d58	cd bb b5	. . .
	ld e,(hl)		;8d5b	5e		^
	inc hl			;8d5c	23		#
	ld d,(hl)		;8d5d	56		V
	push de			;8d5e	d5		.
	pop iy			;8d5f	fd e1		. .
	ld a,(iy+000h)		;8d61	fd 7e 00	. ~ .
	ld (l8db7h),a		;8d64	32 b7 8d	2 . .
	and a			;8d67	a7		.
	ret z			;8d68	c8		.
	ld ix,l8db8h		;8d69	dd 21 b8 8d	. ! . .
l8d6dh:
	push af			;8d6d	f5		.
	ld (ix+001h),006h	;8d6e	dd 36 01 06	. 6 . .
	ld l,(iy+001h)		;8d72	fd 6e 01	. n .
	ld h,(iy+002h)		;8d75	fd 66 02	. f .
	inc iy			;8d78	fd 23		. #
	inc iy			;8d7a	fd 23		. #
	ld (ix+002h),l		;8d7c	dd 75 02	. u .
	ld (ix+004h),h		;8d7f	dd 74 04	. t .
	call sub_b684h		;8d82	cd 84 b6	. . .
	call sub_9910h		;8d85	cd 10 99	. . .
	ld a,(ix+004h)		;8d88	dd 7e 04	. ~ .
	add a,005h		;8d8b	c6 05		. .
	ld (ix+004h),a		;8d8d	dd 77 04	. w .
	call sub_8eb4h		;8d90	cd b4 8e	. . .
	ld a,(l8d48h)		;8d93	3a 48 8d	: H .
	rra			;8d96	1f		.
	jr c,l8da0h		;8d97	38 07		8 .
	ld (ix+001h),007h	;8d99	dd 36 01 07	. 6 . .
	call sub_9910h		;8d9d	cd 10 99	. . .
l8da0h:
	ld a,(ix+002h)		;8da0	dd 7e 02	. ~ .
	add a,005h		;8da3	c6 05		. .
	ld (ix+002h),a		;8da5	dd 77 02	. w .
	ld de,00010h		;8da8	11 10 00	. . .
	add ix,de		;8dab	dd 19		. .
	pop af			;8dad	f1		.
	dec a			;8dae	3d		=
	jr nz,l8d6dh		;8daf	20 bc		  .
	ld a,017h		;8db1	3e 17		> .
	ld (l7939h),a		;8db3	32 39 79	2 9 y
	ret			;8db6	c9		.
l8db7h:
	nop			;8db7	00		.
l8db8h:
	inc bc			;8db8	03		.
	ld b,000h		;8db9	06 00		. .
	nop			;8dbb	00		.
	nop			;8dbc	00		.
	nop			;8dbd	00		.
	nop			;8dbe	00		.
	nop			;8dbf	00		.
	inc b			;8dc0	04		.
	ld e,000h		;8dc1	1e 00		. .
	nop			;8dc3	00		.
	rrca			;8dc4	0f		.
	ld c,000h		;8dc5	0e 00		. .
	nop			;8dc7	00		.
	inc bc			;8dc8	03		.
	ld b,000h		;8dc9	06 00		. .
	nop			;8dcb	00		.
	nop			;8dcc	00		.
	nop			;8dcd	00		.
	nop			;8dce	00		.
	nop			;8dcf	00		.
	inc b			;8dd0	04		.
	ld e,000h		;8dd1	1e 00		. .
	nop			;8dd3	00		.
	rrca			;8dd4	0f		.
	ld c,000h		;8dd5	0e 00		. .
	nop			;8dd7	00		.
	inc bc			;8dd8	03		.
	ld b,000h		;8dd9	06 00		. .
	nop			;8ddb	00		.
	nop			;8ddc	00		.
	nop			;8ddd	00		.
	nop			;8dde	00		.
	nop			;8ddf	00		.
	inc b			;8de0	04		.
	ld e,000h		;8de1	1e 00		. .
	nop			;8de3	00		.
	rrca			;8de4	0f		.
	ld c,000h		;8de5	0e 00		. .
	nop			;8de7	00		.
	inc bc			;8de8	03		.
	ld b,000h		;8de9	06 00		. .
	nop			;8deb	00		.
	nop			;8dec	00		.
	nop			;8ded	00		.
	nop			;8dee	00		.
	nop			;8def	00		.
	inc b			;8df0	04		.
	ld e,000h		;8df1	1e 00		. .
	nop			;8df3	00		.
	rrca			;8df4	0f		.
	ld c,000h		;8df5	0e 00		. .
	nop			;8df7	00		.
	nop			;8df8	00		.
	nop			;8df9	00		.
	nop			;8dfa	00		.
	nop			;8dfb	00		.
	nop			;8dfc	00		.
	nop			;8dfd	00		.
	nop			;8dfe	00		.
	nop			;8dff	00		.
	nop			;8e00	00		.
	nop			;8e01	00		.
	nop			;8e02	00		.
	nop			;8e03	00		.
	nop			;8e04	00		.
	nop			;8e05	00		.
l8e06h:
	ld l,h			;8e06	6c		l
	adc a,(hl)		;8e07	8e		.
	ld l,l			;8e08	6d		m
	adc a,(hl)		;8e09	8e		.
	ld l,h			;8e0a	6c		l
	adc a,(hl)		;8e0b	8e		.
	ld l,c			;8e0c	69		i
	adc a,(hl)		;8e0d	8e		.
	ld l,b			;8e0e	68		h
	adc a,(hl)		;8e0f	8e		.
	ld h,c			;8e10	61		a
	adc a,(hl)		;8e11	8e		.
	ld e,h			;8e12	5c		\
	adc a,(hl)		;8e13	8e		.
	ld d,l			;8e14	55		U
	adc a,(hl)		;8e15	8e		.
	ld c,h			;8e16	4c		L
	adc a,(hl)		;8e17	8e		.
	ld c,c			;8e18	49		I
	adc a,(hl)		;8e19	8e		.
	ld b,h			;8e1a	44		D
	adc a,(hl)		;8e1b	8e		.
	dec a			;8e1c	3d		=
	adc a,(hl)		;8e1d	8e		.
	inc (hl)		;8e1e	34		4
	adc a,(hl)		;8e1f	8e		.
	dec hl			;8e20	2b		+
	adc a,(hl)		;8e21	8e		.
	ld h,08eh		;8e22	26 8e		& .
	ld h,08eh		;8e24	26 8e		& .
	ld (bc),a		;8e26	02		.
	ld c,h			;8e27	4c		L
	add a,d			;8e28	82		.
	sbc a,h			;8e29	9c		.
	add a,d			;8e2a	82		.
	inc b			;8e2b	04		.
	adc a,h			;8e2c	8c		.
	inc h			;8e2d	24		$
	call nz,sub_8c24h	;8e2e	c4 24 8c	. $ .
	ld h,h			;8e31	64		d
	call nz,00464h		;8e32	c4 64 04	. d .
	djnz l8e57h		;8e35	10 20		.  
	ret c			;8e37	d8		.
	jr nz,l8e52h		;8e38	20 18		  .
	ld l,h			;8e3a	6c		l
	ret nc			;8e3b	d0		.
	ld l,h			;8e3c	6c		l
	inc bc			;8e3d	03		.
	ld (hl),h		;8e3e	74		t
	ex af,af'		;8e3f	08		.
	jr nz,l8e86h		;8e40	20 44		  D
	ret z			;8e42	c8		.
	ld b,h			;8e43	44		D
	ld (bc),a		;8e44	02		.
	ld e,h			;8e45	5c		\
	add a,h			;8e46	84		.
	adc a,h			;8e47	8c		.
	add a,h			;8e48	84		.
	ld bc,04474h		;8e49	01 74 44	. t D
	inc b			;8e4c	04		.
	ld b,b			;8e4d	40		@
	inc a			;8e4e	3c		<
	xor b			;8e4f	a8		.
	inc a			;8e50	3c		<
	ld d,h			;8e51	54		T
l8e52h:
	ld l,h			;8e52	6c		l
	sub h			;8e53	94		.
	ld l,h			;8e54	6c		l
	inc bc			;8e55	03		.
	ld (hl),h		;8e56	74		t
l8e57h:
	jr $+78			;8e57	18 4c		. L
	ld (hl),h		;8e59	74		t
	sbc a,h			;8e5a	9c		.
	ld (hl),h		;8e5b	74		t
	ld (bc),a		;8e5c	02		.
	jr nc,l8ebbh		;8e5d	30 5c		0 \
	ret c			;8e5f	d8		.
	ld e,h			;8e60	5c		\
	inc bc			;8e61	03		.
	ld (hl),h		;8e62	74		t
	djnz l8eadh		;8e63	10 48		. H
	ld (hl),e		;8e65	73		s
	and b			;8e66	a0		.
	ld (hl),e		;8e67	73		s
	nop			;8e68	00		.
	ld bc,l7c74h		;8e69	01 74 7c	. t |
	nop			;8e6c	00		.
	ld bc,02c74h		;8e6d	01 74 2c	. t ,
l8e70h:
	nop			;8e70	00		.
l8e71h:
	nop			;8e71	00		.
sub_8e72h:
	ld a,(l8db7h)		;8e72	3a b7 8d	: . .
l8e75h:
	and a			;8e75	a7		.
	ret z			;8e76	c8		.
l8e77h:
	dec a			;8e77	3d		=
	ld b,a			;8e78	47		G
l8e79h:
	push bc			;8e79	c5		.
	call sub_8eb4h		;8e7a	cd b4 8e	. . .
	ld a,(l8d48h)		;8e7d	3a 48 8d	: H .
	and 003h		;8e80	e6 03		. .
	pop bc			;8e82	c1		.
	cp b			;8e83	b8		.
	jr z,l8e88h		;8e84	28 02		( .
l8e86h:
	jr nc,l8e79h		;8e86	30 f1		0 .
l8e88h:
	add a,a			;8e88	87		.
	add a,a			;8e89	87		.
	add a,a			;8e8a	87		.
	add a,a			;8e8b	87		.
	ld hl,l8db8h		;8e8c	21 b8 8d	! . .
	call sub_b5bbh		;8e8f	cd bb b5	. . .
	ld (l8e70h),hl		;8e92	22 70 8e	" p .
	push hl			;8e95	e5		.
	pop ix			;8e96	dd e1		. .
	ld a,(ix+001h)		;8e98	dd 7e 01	. ~ .
	xor 001h		;8e9b	ee 01		. .
	ld (ix+001h),a		;8e9d	dd 77 01	. w .
	ld a,(ix+002h)		;8ea0	dd 7e 02	. ~ .
	push af			;8ea3	f5		.
	sub 005h		;8ea4	d6 05		. .
	ld (ix+002h),a		;8ea6	dd 77 02	. w .
	call sub_9910h		;8ea9	cd 10 99	. . .
	pop af			;8eac	f1		.
l8eadh:
	ld (ix+002h),a		;8ead	dd 77 02	. w .
	call sub_c151h		;8eb0	cd 51 c1	. Q .
	ret			;8eb3	c9		.
sub_8eb4h:
	ld de,(l8d48h)		;8eb4	ed 5b 48 8d	. [ H .
	ld hl,(l8d4ah)		;8eb8	2a 4a 8d	* J .
l8ebbh:
	ld a,(hl)		;8ebb	7e		~
	add a,005h		;8ebc	c6 05		. .
	ld b,a			;8ebe	47		G
	ld a,(l8ed9h)		;8ebf	3a d9 8e	: . .
	add a,b			;8ec2	80		.
	add a,e			;8ec3	83		.
	ld e,a			;8ec4	5f		_
	ld a,(hl)		;8ec5	7e		~
	cpl			;8ec6	2f		/
	add a,016h		;8ec7	c6 16		. .
	add a,d			;8ec9	82		.
	add a,l			;8eca	85		.
	ld d,a			;8ecb	57		W
	ld (l8d48h),de		;8ecc	ed 53 48 8d	. S H .
	inc hl			;8ed0	23		#
	ld a,h			;8ed1	7c		|
	and 09fh		;8ed2	e6 9f		. .
	ld h,a			;8ed4	67		g
	ld (l8d4ah),hl		;8ed5	22 4a 8d	" J .
	ret			;8ed8	c9		.
l8ed9h:
	ld b,036h		;8ed9	06 36		. 6
	nop			;8edb	00		.
	inc hl			;8edc	23		#
	djnz $-3		;8edd	10 fb		. .
	ret			;8edf	c9		.
l8ee0h:
	dec d			;8ee0	15		.
	ret nz			;8ee1	c0		.
	ret pe			;8ee2	e8		.
	adc a,(hl)		;8ee3	8e		.
	djnz l8e75h		;8ee4	10 8f		. .
	jr c,l8e77h		;8ee6	38 8f		8 .
	ld (bc),a		;8ee8	02		.
	djnz l8eebh		;8ee9	10 00		. .
l8eebh:
	nop			;8eeb	00		.
	rst 38h			;8eec	ff		.
	cp 0bbh			;8eed	fe bb		. .
	cp d			;8eef	ba		.
	rst 38h			;8ef0	ff		.
	cp 0ffh			;8ef1	fe ff		. .
	cp 0bfh			;8ef3	fe bf		. .
	jp m,0fefbh		;8ef5	fa fb fe	. . .
	ei			;8ef8	fb		.
	cp 0bbh			;8ef9	fe bb		. .
	jp m,0fefbh		;8efb	fa fb fe	. . .
	ret m			;8efe	f8		.
	ld a,0bfh		;8eff	3e bf		> .
	jp m,0feffh		;8f01	fa ff fe	. . .
	rst 38h			;8f04	ff		.
	cp 0bbh			;8f05	fe bb		. .
	cp d			;8f07	ba		.
	rst 38h			;8f08	ff		.
	cp 002h			;8f09	fe 02		. .
	ld (bc),a		;8f0b	02		.
	ld b,h			;8f0c	44		D
	ld b,h			;8f0d	44		D
	ld b,h			;8f0e	44		D
	ld b,h			;8f0f	44		D
	ld (bc),a		;8f10	02		.
	djnz $-7		;8f11	10 f7		. .
	rst 30h			;8f13	f7		.
	rst 30h			;8f14	f7		.
	rst 30h			;8f15	f7		.
	rst 28h			;8f16	ef		.
	ei			;8f17	fb		.
	rst 28h			;8f18	ef		.
	ei			;8f19	fb		.
	rst 18h			;8f1a	df		.
	defb 0fdh,0dfh,0fdh ;illegal sequence	;8f1b	fd df fd	. . .
	cp a			;8f1e	bf		.
	cp 07fh			;8f1f	fe 7f		. .
	rst 38h			;8f21	ff		.
	cp a			;8f22	bf		.
	cp 0dfh			;8f23	fe df		. .
	defb 0fdh,0dfh,0fdh ;illegal sequence	;8f25	fd df fd	. . .
	rst 28h			;8f28	ef		.
	ei			;8f29	fb		.
	rst 28h			;8f2a	ef		.
	ei			;8f2b	fb		.
	rst 30h			;8f2c	f7		.
	rst 30h			;8f2d	f7		.
	rst 30h			;8f2e	f7		.
	rst 30h			;8f2f	f7		.
	ret m			;8f30	f8		.
	rrca			;8f31	0f		.
	ld (bc),a		;8f32	02		.
	ld (bc),a		;8f33	02		.
	ld b,l			;8f34	45		E
	ld b,l			;8f35	45		E
	ld b,l			;8f36	45		E
	ld b,l			;8f37	45		E
	ld (bc),a		;8f38	02		.
	djnz $-39		;8f39	10 d7		. .
	rst 10h			;8f3b	d7		.
	cp e			;8f3c	bb		.
	cp e			;8f3d	bb		.
	ld a,l			;8f3e	7d		}
	ld a,l			;8f3f	7d		}
	cp 0feh			;8f40	fe fe		. .
	ld a,a			;8f42	7f		.
	defb 0fdh,0bfh,0fbh ;illegal sequence	;8f43	fd bf fb	. . .
	rst 18h			;8f46	df		.
	rst 30h			;8f47	f7		.
	rst 28h			;8f48	ef		.
	rst 28h			;8f49	ef		.
	rst 18h			;8f4a	df		.
	rst 30h			;8f4b	f7		.
	cp a			;8f4c	bf		.
	ei			;8f4d	fb		.
	ld a,a			;8f4e	7f		.
	defb 0fdh,0feh,0feh ;illegal sequence	;8f4f	fd fe fe	. . .
	ld a,l			;8f52	7d		}
	ld a,l			;8f53	7d		}
	cp e			;8f54	bb		.
	cp e			;8f55	bb		.
	rst 10h			;8f56	d7		.
	rst 10h			;8f57	d7		.
	rst 28h			;8f58	ef		.
	rst 28h			;8f59	ef		.
	ld (bc),a		;8f5a	02		.
	ld (bc),a		;8f5b	02		.
	ld b,a			;8f5c	47		G
	ld b,a			;8f5d	47		G
	ld b,a			;8f5e	47		G
	ld b,a			;8f5f	47		G
sub_8f60h:
	ld hl,0a458h		;8f60	21 58 a4	! X .
	call sub_b57dh		;8f63	cd 7d b5	. } .
	ld c,020h		;8f66	0e 20		.  
l8f68h:
	ld b,00ah		;8f68	06 0a		. .
	push hl			;8f6a	e5		.
l8f6bh:
	ld (hl),000h		;8f6b	36 00		6 .
	inc l			;8f6d	2c		,
	djnz l8f6bh		;8f6e	10 fb		. .
	pop hl			;8f70	e1		.
	call sub_b56eh		;8f71	cd 6e b5	. n .
	dec c			;8f74	0d		.
	jr nz,l8f68h		;8f75	20 f1		  .
	ld a,(lb7ebh)		;8f77	3a eb b7	: . .
	inc a			;8f7a	3c		<
	ld b,a			;8f7b	47		G
	xor a			;8f7c	af		.
l8f7dh:
	add a,001h		;8f7d	c6 01		. .
	daa			;8f7f	27		'
	djnz l8f7dh		;8f80	10 fb		. .
	ld b,a			;8f82	47		G
	rra			;8f83	1f		.
	rra			;8f84	1f		.
	rra			;8f85	1f		.
	rra			;8f86	1f		.
	and 00fh		;8f87	e6 0f		. .
	ld (08fc3h),a		;8f89	32 c3 8f	2 . .
	ld a,b			;8f8c	78		x
	and 00fh		;8f8d	e6 0f		. .
	ld (l8fc4h),a		;8f8f	32 c4 8f	2 . .
	ld a,(lb7e6h)		;8f92	3a e6 b7	: . .
	inc a			;8f95	3c		<
	ld (08fb8h),a		;8f96	32 b8 8f	2 . .
	ld de,l8fadh		;8f99	11 ad 8f	. . .
	ld b,002h		;8f9c	06 02		. .
	call sub_b796h		;8f9e	cd 96 b7	. . .
	ld a,(lb7e5h)		;8fa1	3a e5 b7	: . .
	cp 002h			;8fa4	fe 02		. .
	ret nz			;8fa6	c0		.
	ld de,l8fc5h		;8fa7	11 c5 8f	. . .
	jp lb4ech		;8faa	c3 ec b4	. . .
l8fadh:
	ld h,b			;8fad	60		`
	adc a,a			;8fae	8f		.
	ld b,a			;8faf	47		G
	ex af,af'		;8fb0	08		.
	add hl,de		;8fb1	19		.
	dec d			;8fb2	15		.
	ld a,(bc)		;8fb3	0a		.
	ld (01b0eh),hl		;8fb4	22 0e 1b	" . .
	ld h,000h		;8fb7	26 00		& .
	ld h,b			;8fb9	60		`
	sbc a,(hl)		;8fba	9e		.
	ld b,a			;8fbb	47		G
	ex af,af'		;8fbc	08		.
	dec de			;8fbd	1b		.
	jr $+32			;8fbe	18 1e		. .
	rla			;8fc0	17		.
	dec c			;8fc1	0d		.
	ld h,000h		;8fc2	26 00		& .
l8fc4h:
	nop			;8fc4	00		.
l8fc5h:
	ld h,b			;8fc5	60		`
	adc a,a			;8fc6	8f		.
	ld b,a			;8fc7	47		G
	ex af,af'		;8fc8	08		.
	djnz l8fd5h		;8fc9	10 0a		. .
	ld d,00eh		;8fcb	16 0e		. .
	ld h,026h		;8fcd	26 26		& &
	jr l8fe8h		;8fcf	18 17		. .
l8fd1h:
	jr c,l8ff9h		;8fd1	38 26		8 &
	ld b,a			;8fd3	47		G
	ld (bc),a		;8fd4	02		.
l8fd5h:
	ld bc,03824h		;8fd5	01 24 38	. $ 8
	ld (hl),047h		;8fd8	36 47		6 G
	ld (bc),a		;8fda	02		.
	ld (bc),a		;8fdb	02		.
	inc h			;8fdc	24		$
	jr c,l9025h		;8fdd	38 46		8 F
	ld b,(hl)		;8fdf	46		F
	ld (bc),a		;8fe0	02		.
	inc bc			;8fe1	03		.
	inc h			;8fe2	24		$
	jr c,l903bh		;8fe3	38 56		8 V
	ld b,(hl)		;8fe5	46		F
	ld (bc),a		;8fe6	02		.
	inc b			;8fe7	04		.
l8fe8h:
	inc h			;8fe8	24		$
	jr c,$+104		;8fe9	38 66		8 f
	ld b,l			;8feb	45		E
	ld (bc),a		;8fec	02		.
	dec b			;8fed	05		.
	inc h			;8fee	24		$
	jr c,$+120		;8fef	38 76		8 v
	ld b,l			;8ff1	45		E
	ld (bc),a		;8ff2	02		.
	ld b,024h		;8ff3	06 24		. $
	jr c,l8f7dh		;8ff5	38 86		8 .
	ld b,h			;8ff7	44		D
	ld (bc),a		;8ff8	02		.
l8ff9h:
	rlca			;8ff9	07		.
	inc h			;8ffa	24		$
	jr c,$-104		;8ffb	38 96		8 .
	ld b,h			;8ffd	44		D
	ld (bc),a		;8ffe	02		.
	ex af,af'		;8fff	08		.
	inc h			;9000	24		$
	jr c,$-88		;9001	38 a6		8 .
	ld b,e			;9003	43		C
	ld (bc),a		;9004	02		.
	add hl,bc		;9005	09		.
	inc h			;9006	24		$
	jr nc,$-72		;9007	30 b6		0 .
	ld b,e			;9009	43		C
	inc bc			;900a	03		.
	ld bc,02400h		;900b	01 00 24	. . $
	ld e,b			;900e	58		X
	ld h,047h		;900f	26 47		& G
	ld c,001h		;9011	0e 01		. .
	nop			;9013	00		.
	nop			;9014	00		.
	nop			;9015	00		.
	nop			;9016	00		.
	nop			;9017	00		.
	ld h,026h		;9018	26 26		& &
	ld h,011h		;901a	26 11		& .
	ld h,012h		;901c	26 12		& .
	ld h,01dh		;901e	26 1d		& .
	ld e,b			;9020	58		X
	ld (hl),007h		;9021	36 07		6 .
	ld c,000h		;9023	0e 00		. .
l9025h:
	add hl,bc		;9025	09		.
	nop			;9026	00		.
	nop			;9027	00		.
	nop			;9028	00		.
	nop			;9029	00		.
	ld h,026h		;902a	26 26		& &
	ld h,019h		;902c	26 19		& .
	ld h,00ah		;902e	26 0a		& .
	ld h,014h		;9030	26 14		& .
	ld e,b			;9032	58		X
	ld b,(hl)		;9033	46		F
	ld b,(hl)		;9034	46		F
	ld c,000h		;9035	0e 00		. .
	ex af,af'		;9037	08		.
	nop			;9038	00		.
	nop			;9039	00		.
	nop			;903a	00		.
l903bh:
	nop			;903b	00		.
	ld h,026h		;903c	26 26		& &
	ld h,011h		;903e	26 11		& .
	ld h,012h		;9040	26 12		& .
	ld h,01dh		;9042	26 1d		& .
	ld e,b			;9044	58		X
	ld d,(hl)		;9045	56		V
	ld b,00eh		;9046	06 0e		. .
	nop			;9048	00		.
	rlca			;9049	07		.
	nop			;904a	00		.
	nop			;904b	00		.
	nop			;904c	00		.
	nop			;904d	00		.
	ld h,026h		;904e	26 26		& &
	ld h,019h		;9050	26 19		& .
	ld h,00ah		;9052	26 0a		& .
	ld h,014h		;9054	26 14		& .
	ld e,b			;9056	58		X
	ld h,(hl)		;9057	66		f
	ld b,l			;9058	45		E
	ld c,000h		;9059	0e 00		. .
	ld b,000h		;905b	06 00		. .
	nop			;905d	00		.
	nop			;905e	00		.
	nop			;905f	00		.
	ld h,026h		;9060	26 26		& &
	ld h,011h		;9062	26 11		& .
	ld h,012h		;9064	26 12		& .
	ld h,01dh		;9066	26 1d		& .
	ld e,b			;9068	58		X
	halt			;9069	76		v
	dec b			;906a	05		.
	ld c,000h		;906b	0e 00		. .
	dec b			;906d	05		.
	nop			;906e	00		.
	nop			;906f	00		.
	nop			;9070	00		.
	nop			;9071	00		.
	ld h,026h		;9072	26 26		& &
	ld h,019h		;9074	26 19		& .
	ld h,00ah		;9076	26 0a		& .
	ld h,014h		;9078	26 14		& .
	ld e,b			;907a	58		X
	add a,(hl)		;907b	86		.
	ld b,h			;907c	44		D
	ld c,000h		;907d	0e 00		. .
	inc b			;907f	04		.
	nop			;9080	00		.
	nop			;9081	00		.
	nop			;9082	00		.
	nop			;9083	00		.
	ld h,026h		;9084	26 26		& &
	ld h,011h		;9086	26 11		& .
	ld h,012h		;9088	26 12		& .
	ld h,01dh		;908a	26 1d		& .
	ld e,b			;908c	58		X
	sub (hl)		;908d	96		.
	inc b			;908e	04		.
	ld c,000h		;908f	0e 00		. .
	inc bc			;9091	03		.
	nop			;9092	00		.
	nop			;9093	00		.
	nop			;9094	00		.
	nop			;9095	00		.
	ld h,026h		;9096	26 26		& &
	ld h,019h		;9098	26 19		& .
	ld h,00ah		;909a	26 0a		& .
	ld h,014h		;909c	26 14		& .
	ld e,b			;909e	58		X
	and (hl)		;909f	a6		.
	ld b,e			;90a0	43		C
	ld c,000h		;90a1	0e 00		. .
	ld (bc),a		;90a3	02		.
	nop			;90a4	00		.
	nop			;90a5	00		.
	nop			;90a6	00		.
	nop			;90a7	00		.
	ld h,026h		;90a8	26 26		& &
	ld h,011h		;90aa	26 11		& .
	ld h,012h		;90ac	26 12		& .
	ld h,01dh		;90ae	26 1d		& .
	ld e,b			;90b0	58		X
	or (hl)			;90b1	b6		.
	inc bc			;90b2	03		.
	ld c,000h		;90b3	0e 00		. .
	ld bc,00000h		;90b5	01 00 00	. . .
	nop			;90b8	00		.
	nop			;90b9	00		.
	ld h,026h		;90ba	26 26		& &
	ld h,019h		;90bc	26 19		& .
	ld h,00ah		;90be	26 0a		& .
	ld h,014h		;90c0	26 14		& .
	ld d,b			;90c2	50		P
	rrca			;90c3	0f		.
	ld b,a			;90c4	47		G
	inc c			;90c5	0c		.
	ld de,01012h		;90c6	11 12 10	. . .
	ld de,02626h		;90c9	11 26 26	. & &
	inc e			;90cc	1c		.
	inc c			;90cd	0c		.
	jr l90ebh		;90ce	18 1b		. .
	ld c,01ch		;90d0	0e 1c		. .
	ld d,b			;90d2	50		P
	ld d,046h		;90d3	16 46		. F
	inc c			;90d5	0c		.
	ld hl,(02a2ah)		;90d6	2a 2a 2a	* * *
	ld hl,(02a2ah)		;90d9	2a 2a 2a	* * *
	ld hl,(02a2ah)		;90dc	2a 2a 2a	* * *
	ld hl,(02a2ah)		;90df	2a 2a 2a	* * *
l90e2h:
	nop			;90e2	00		.
	nop			;90e3	00		.
	nop			;90e4	00		.
	nop			;90e5	00		.
	nop			;90e6	00		.
	nop			;90e7	00		.
l90e8h:
	ld d,b			;90e8	50		P
	rrca			;90e9	0f		.
	ld b,a			;90ea	47		G
l90ebh:
	inc c			;90eb	0c		.
	ld h,026h		;90ec	26 26		& &
	add hl,de		;90ee	19		.
	dec d			;90ef	15		.
	ld a,(bc)		;90f0	0a		.
	ld (01b0eh),hl		;90f1	22 0e 1b	" . .
	ld h,000h		;90f4	26 00		& .
	ld h,026h		;90f6	26 26		& &
	ld b,b			;90f8	40		@
	rla			;90f9	17		.
	ld b,a			;90fa	47		G
	djnz l910bh		;90fb	10 0e		. .
	rla			;90fd	17		.
	dec e			;90fe	1d		.
l90ffh:
	ld c,01bh		;90ff	0e 1b		. .
	ld h,022h		;9101	26 22		& "
	jr l9123h		;9103	18 1e		. .
	dec de			;9105	1b		.
	ld h,017h		;9106	26 17		& .
	ld a,(bc)		;9108	0a		.
	ld d,00eh		;9109	16 0e		. .
l910bh:
	inc h			;910b	24		$
sub_910ch:
	di			;910c	f3		.
	ld ix,090b4h		;910d	dd 21 b4 90	. ! . .
	ld hl,lb7eeh		;9111	21 ee b7	! . .
	ld de,l90e2h		;9114	11 e2 90	. . .
	ld b,003h		;9117	06 03		. .
l9119h:
	ld a,(hl)		;9119	7e		~
	rra			;911a	1f		.
	rra			;911b	1f		.
	rra			;911c	1f		.
	rra			;911d	1f		.
	and 00fh		;911e	e6 0f		. .
	ld (de),a		;9120	12		.
	inc de			;9121	13		.
	ld a,(hl)		;9122	7e		~
l9123h:
	and 00fh		;9123	e6 0f		. .
	ld (de),a		;9125	12		.
	inc de			;9126	13		.
	dec hl			;9127	2b		+
	djnz l9119h		;9128	10 ef		. .
	ld iy,l90e2h		;912a	fd 21 e2 90	. ! . .
	ld d,00bh		;912e	16 0b		. .
l9130h:
	ld b,006h		;9130	06 06		. .
	push iy			;9132	fd e5		. .
	push ix			;9134	dd e5		. .
l9136h:
	ld a,(ix+000h)		;9136	dd 7e 00	. ~ .
	cp (iy+000h)		;9139	fd be 00	. . .
	jr c,l9150h		;913c	38 12		8 .
	jr nz,l9146h		;913e	20 06		  .
	inc ix			;9140	dd 23		. #
	inc iy			;9142	fd 23		. #
	djnz l9136h		;9144	10 f0		. .
l9146h:
	pop ix			;9146	dd e1		. .
	pop iy			;9148	fd e1		. .
	ld a,d			;914a	7a		z
	cp 00bh			;914b	fe 0b		. .
	ret z			;914d	c8		.
	jr l915eh		;914e	18 0e		. .
l9150h:
	dec d			;9150	15		.
	pop ix			;9151	dd e1		. .
	pop iy			;9153	fd e1		. .
	ld bc,0ffeeh		;9155	01 ee ff	. . .
	add ix,bc		;9158	dd 09		. .
	ld a,d			;915a	7a		z
	dec a			;915b	3d		=
	jr nz,l9130h		;915c	20 d2		  .
l915eh:
	ld a,00ah		;915e	3e 0a		> .
	sub d			;9160	92		.
	jr z,l9179h		;9161	28 16		( .
	ld de,090c1h		;9163	11 c1 90	. . .
	ld hl,090afh		;9166	21 af 90	! . .
l9169h:
	ld bc,0000eh		;9169	01 0e 00	. . .
	lddr			;916c	ed b8		. .
	dec hl			;916e	2b		+
	dec hl			;916f	2b		+
	dec hl			;9170	2b		+
	dec hl			;9171	2b		+
	dec de			;9172	1b		.
	dec de			;9173	1b		.
	dec de			;9174	1b		.
	dec de			;9175	1b		.
	dec a			;9176	3d		=
	jr nz,l9169h		;9177	20 f0		  .
l9179h:
	ld de,00012h		;9179	11 12 00	. . .
	add ix,de		;917c	dd 19		. .
	push ix			;917e	dd e5		. .
	pop de			;9180	d1		.
	ld hl,l90e2h		;9181	21 e2 90	! . .
	ld bc,00006h		;9184	01 06 00	. . .
	ldir			;9187	ed b0		. .
	ld c,026h		;9189	0e 26		. &
	ld (ix+009h),00ah	;918b	dd 36 09 0a	. 6 . .
	ld (ix+00ah),c		;918f	dd 71 0a	. q .
	ld (ix+00bh),c		;9192	dd 71 0b	. q .
	ld (ix+00ch),c		;9195	dd 71 0c	. q .
	ld (ix+00dh),c		;9198	dd 71 0d	. q .
	call sub_97adh		;919b	cd ad 97	. . .
	call sub_97bch		;919e	cd bc 97	. . .
	call sub_926bh		;91a1	cd 6b 92	. k .
	ld a,(lb7e6h)		;91a4	3a e6 b7	: . .
	inc a			;91a7	3c		<
	ld (090f5h),a		;91a8	32 f5 90	2 . .
	ld de,l90e8h		;91ab	11 e8 90	. . .
	call lb4ech		;91ae	cd ec b4	. . .
	call lb4ech		;91b1	cd ec b4	. . .
	push ix			;91b4	dd e5		. .
	pop hl			;91b6	e1		.
	dec hl			;91b7	2b		+
	dec hl			;91b8	2b		+
	dec hl			;91b9	2b		+
	dec hl			;91ba	2b		+
	ld (l9229h),hl		;91bb	22 29 92	" ) .
	ld b,005h		;91be	06 05		. .
l91c0h:
	ld c,00ah		;91c0	0e 0a		. .
	push bc			;91c2	c5		.
l91c3h:
	call sub_a1dbh		;91c3	cd db a1	. . .
	ld a,(l8ed9h)		;91c6	3a d9 8e	: . .
	and 013h		;91c9	e6 13		. .
	jr z,l91c3h		;91cb	28 f6		( .
	bit 4,a			;91cd	cb 67		. g
	jr nz,l91ffh		;91cf	20 2e		  .
	cp 003h			;91d1	fe 03		. .
	jr z,l91c3h		;91d3	28 ee		( .
	pop bc			;91d5	c1		.
	rra			;91d6	1f		.
	jr nc,l91e3h		;91d7	30 0a		0 .
	inc c			;91d9	0c		.
	ld a,c			;91da	79		y
	cp 028h			;91db	fe 28		. (
	jr nz,l91eah		;91dd	20 0b		  .
	ld c,000h		;91df	0e 00		. .
	jr l91eah		;91e1	18 07		. .
l91e3h:
	dec c			;91e3	0d		.
	bit 7,c			;91e4	cb 79		. y
	jr z,l91eah		;91e6	28 02		( .
	ld c,027h		;91e8	0e 27		. '
l91eah:
	ld (ix+009h),c		;91ea	dd 71 09	. q .
	push bc			;91ed	c5		.
	ld de,(l9229h)		;91ee	ed 5b 29 92	. [ ) .
	call lb4ech		;91f2	cd ec b4	. . .
	call sub_c159h		;91f5	cd 59 c1	. Y .
	ld d,020h		;91f8	16 20		.  
	call sub_97d3h		;91fa	cd d3 97	. . .
	jr l91c3h		;91fd	18 c4		. .
l91ffh:
	call sub_c168h		;91ff	cd 68 c1	. h .
	pop bc			;9202	c1		.
	dec b			;9203	05		.
	jr z,l9221h		;9204	28 1b		( .
	inc ix			;9206	dd 23		. #
	ld (ix+009h),00ah	;9208	dd 36 09 0a	. 6 . .
	push bc			;920c	c5		.
	ld de,(l9229h)		;920d	ed 5b 29 92	. [ ) .
	call lb4ech		;9211	cd ec b4	. . .
l9214h:
	call sub_a1dbh		;9214	cd db a1	. . .
	ld a,(l8ed9h)		;9217	3a d9 8e	: . .
	and 010h		;921a	e6 10		. .
	jr nz,l9214h		;921c	20 f6		  .
	pop bc			;921e	c1		.
	jr l91c0h		;921f	18 9f		. .
l9221h:
	call sub_926bh		;9221	cd 6b 92	. k .
	ld b,00ah		;9224	06 0a		. .
	jp lb7dch		;9226	c3 dc b7	. . .
l9229h:
	nop			;9229	00		.
	nop			;922a	00		.
l922bh:
	jr nz,l9245h		;922b	20 18		  .
	ld b,e			;922d	43		C
l922eh:
	jr nz,$+26		;922e	20 18		  .
	ld b,d			;9230	42		B
sub_9231h:
	ld hl,00000h		;9231	21 00 00	! . .
	ld (l8d46h),hl		;9234	22 46 8d	" F .
	call sub_97adh		;9237	cd ad 97	. . .
	call sub_97bch		;923a	cd bc 97	. . .
	ld hl,057e0h		;923d	21 e0 57	! . W
	ld de,057ffh		;9240	11 ff 57	. . W
	ld b,0c0h		;9243	06 c0		. .
l9245h:
	ld (hl),0c0h		;9245	36 c0		6 .
	ld a,003h		;9247	3e 03		> .
	ld (de),a		;9249	12		.
	call sub_b56eh		;924a	cd 6e b5	. n .
	ex de,hl		;924d	eb		.
	call sub_b56eh		;924e	cd 6e b5	. n .
	ex de,hl		;9251	eb		.
	djnz l9245h		;9252	10 f1		. .
	ld hl,056e0h		;9254	21 e0 56	! . V
	ld de,04000h		;9257	11 00 40	. . @
	ld b,020h		;925a	06 20		.  
	ld a,0ffh		;925c	3e ff		> .
l925eh:
	ld (hl),a		;925e	77		w
	inc h			;925f	24		$
	ld (hl),a		;9260	77		w
	dec h			;9261	25		%
	ld (de),a		;9262	12		.
	inc d			;9263	14		.
	ld (de),a		;9264	12		.
	dec d			;9265	15		.
	inc l			;9266	2c		,
	inc e			;9267	1c		.
	djnz l925eh		;9268	10 f4		. .
	ret			;926a	c9		.
sub_926bh:
	call sub_9231h		;926b	cd 31 92	. 1 .
	ld hl,lbf00h		;926e	21 00 bf	! . .
	ld de,l922eh		;9271	11 2e 92	. . .
	call sub_b61ch		;9274	cd 1c b6	. . .
	ld de,l8fd1h		;9277	11 d1 8f	. . .
	ld b,016h		;927a	06 16		. .
	jp sub_b796h		;927c	c3 96 b7	. . .
l927fh:
	call sub_926bh		;927f	cd 6b 92	. k .
l9282h:
	ld a,0efh		;9282	3e ef		> .
	call sub_97a7h		;9284	cd a7 97	. . .
	rra			;9287	1f		.
	ret c			;9288	d8		.
	xor a			;9289	af		.
	call sub_97a7h		;928a	cd a7 97	. . .
	jp nz,093f8h		;928d	c2 f8 93	. . .
	ld a,080h		;9290	3e 80		> .
l9292h:
	dec a			;9292	3d		=
	jr nz,l9292h		;9293	20 fd		  .
	ld hl,(l8d46h)		;9295	2a 46 8d	* F .
	inc hl			;9298	23		#
	ld (l8d46h),hl		;9299	22 46 8d	" F .
	bit 6,h			;929c	cb 74		. t
	jp nz,093f8h		;929e	c2 f8 93	. . .
	jr l9282h		;92a1	18 df		. .
sub_92a3h:
	ld b,003h		;92a3	06 03		. .
l92a5h:
	ld a,(hl)		;92a5	7e		~
	rra			;92a6	1f		.
	rra			;92a7	1f		.
	rra			;92a8	1f		.
	rra			;92a9	1f		.
	and 00fh		;92aa	e6 0f		. .
	ld (de),a		;92ac	12		.
	inc de			;92ad	13		.
	ld a,(hl)		;92ae	7e		~
	and 00fh		;92af	e6 0f		. .
	ld (de),a		;92b1	12		.
	inc de			;92b2	13		.
	dec hl			;92b3	2b		+
	djnz l92a5h		;92b4	10 ef		. .
	ret			;92b6	c9		.
	inc b			;92b7	04		.
	inc bc			;92b8	03		.
	ld b,a			;92b9	47		G
	inc b			;92ba	04		.
	inc bc			;92bb	03		.
	nop			;92bc	00		.
l92bdh:
	jr z,$+110		;92bd	28 6c		( l
l92bfh:
	cp b			;92bf	b8		.
l92c0h:
	ld l,h			;92c0	6c		l
l92c1h:
	inc b			;92c1	04		.
	djnz l92cbh		;92c2	10 07		. .
	rst 38h			;92c4	ff		.
	ret m			;92c5	f8		.
	nop			;92c6	00		.
	ld e,003h		;92c7	1e 03		. .
	rst 20h			;92c9	e7		.
	nop			;92ca	00		.
l92cbh:
	dec a			;92cb	3d		=
	defb 0fdh,0dfh,0c0h ;illegal sequence	;92cc	fd df c0	. . .
	dec a			;92cf	3d		=
	defb 0fdh,0bfh,0e0h ;illegal sequence	;92d0	fd bf e0	. . .
	ld a,(hl)		;92d3	7e		~
	ld (hl),e		;92d4	73		s
	cp h			;92d5	bc		.
	jr nz,l9357h		;92d6	20 7f		  .
	ld (hl),a		;92d8	77		w
	ld a,e			;92d9	7b		{
	ret nz			;92da	c0		.
	rst 38h			;92db	ff		.
	ld (hl),a		;92dc	77		w
	ld (hl),a		;92dd	77		w
	ret m			;92de	f8		.
	rst 38h			;92df	ff		.
	ld (hl),a		;92e0	77		w
	ld l,e			;92e1	6b		k
	rst 38h			;92e2	ff		.
	rst 8			;92e3	cf		.
	ld (hl),a		;92e4	77		w
	ld l,c			;92e5	69		i
	rst 0			;92e6	c7		.
	adc a,077h		;92e7	ce 77		. w
	ld (hl),h		;92e9	74		t
	jr c,$+71		;92ea	38 45		8 E
	rst 30h			;92ec	f7		.
	ld a,e			;92ed	7b		{
	ret nz			;92ee	c0		.
	ld h,l			;92ef	65		e
	rst 30h			;92f0	f7		.
	cp h			;92f1	bc		.
	jr nz,$+36		;92f2	20 22		  "
	rst 30h			;92f4	f7		.
	cp a			;92f5	bf		.
	ret po			;92f6	e0		.
	jr nc,l9369h		;92f7	30 70		0 p
	ld e,a			;92f9	5f		_
	ret nz			;92fa	c0		.
	inc e			;92fb	1c		.
	nop			;92fc	00		.
	daa			;92fd	27		'
	nop			;92fe	00		.
	rlca			;92ff	07		.
	rst 38h			;9300	ff		.
	ret m			;9301	f8		.
	nop			;9302	00		.
l9303h:
	inc b			;9303	04		.
	djnz l9306h		;9304	10 00		. .
l9306h:
	rra			;9306	1f		.
	rst 38h			;9307	ff		.
	ret po			;9308	e0		.
	nop			;9309	00		.
	rst 20h			;930a	e7		.
	ret nz			;930b	c0		.
	jr c,$+5		;930c	38 03		8 .
	ei			;930e	fb		.
	cp a			;930f	bf		.
	call c,0fd07h		;9310	dc 07 fd	. . .
	cp a			;9313	bf		.
	call c,03d04h		;9314	dc 04 3d	. . =
	cp h			;9317	bc		.
	ld a,003h		;9318	3e 03		> .
	sbc a,0deh		;931a	de de		. .
	cp 01fh			;931c	fe 1f		. .
	xor 0efh		;931e	ee ef		. .
	ld a,a			;9320	7f		.
	rst 38h			;9321	ff		.
	or 0f7h			;9322	f6 f7		. .
	cp a			;9324	bf		.
	ex (sp),hl		;9325	e3		.
	or 0cbh			;9326	f6 cb		. .
	rst 18h			;9328	df		.
	inc e			;9329	1c		.
	ld l,0b5h		;932a	2e b5		. .
	rst 18h			;932c	df		.
	inc bc			;932d	03		.
	sbc a,0b9h		;932e	de b9		. .
	sbc a,004h		;9330	de 04		. .
	dec a			;9332	3d		=
	cp a			;9333	bf		.
	sub 007h		;9334	d6 07		. .
	defb 0fdh,0dfh,084h ;illegal sequence	;9336	fd df 84	. . .
	inc bc			;9339	03		.
	jp m,00c0fh		;933a	fa 0f 0c	. . .
	nop			;933d	00		.
	call po,03800h		;933e	e4 00 38	. . 8
	nop			;9341	00		.
	rra			;9342	1f		.
	rst 38h			;9343	ff		.
	ret po			;9344	e0		.
l9345h:
	inc b			;9345	04		.
	djnz l9348h		;9346	10 00		. .
l9348h:
	nop			;9348	00		.
	nop			;9349	00		.
	nop			;934a	00		.
	nop			;934b	00		.
	nop			;934c	00		.
	nop			;934d	00		.
	nop			;934e	00		.
	nop			;934f	00		.
	nop			;9350	00		.
	nop			;9351	00		.
	nop			;9352	00		.
	nop			;9353	00		.
	nop			;9354	00		.
	nop			;9355	00		.
	nop			;9356	00		.
l9357h:
	nop			;9357	00		.
	nop			;9358	00		.
	nop			;9359	00		.
	nop			;935a	00		.
	nop			;935b	00		.
	nop			;935c	00		.
	nop			;935d	00		.
	nop			;935e	00		.
	nop			;935f	00		.
	nop			;9360	00		.
	nop			;9361	00		.
	nop			;9362	00		.
	nop			;9363	00		.
	nop			;9364	00		.
	nop			;9365	00		.
	nop			;9366	00		.
	nop			;9367	00		.
	nop			;9368	00		.
l9369h:
	nop			;9369	00		.
	nop			;936a	00		.
	nop			;936b	00		.
	nop			;936c	00		.
	nop			;936d	00		.
	nop			;936e	00		.
	nop			;936f	00		.
	nop			;9370	00		.
	nop			;9371	00		.
	nop			;9372	00		.
	nop			;9373	00		.
	nop			;9374	00		.
	nop			;9375	00		.
	nop			;9376	00		.
	nop			;9377	00		.
	nop			;9378	00		.
	nop			;9379	00		.
	nop			;937a	00		.
	nop			;937b	00		.
	nop			;937c	00		.
	nop			;937d	00		.
	nop			;937e	00		.
	nop			;937f	00		.
	nop			;9380	00		.
	nop			;9381	00		.
	nop			;9382	00		.
	nop			;9383	00		.
	nop			;9384	00		.
	nop			;9385	00		.
	nop			;9386	00		.
l9387h:
	ld e,016h		;9387	1e 16		. .
	ld b,a			;9389	47		G
l938ah:
	nop			;938a	00		.
l938bh:
	nop			;938b	00		.
l938ch:
	inc b			;938c	04		.
	dec c			;938d	0d		.
	nop			;938e	00		.
	nop			;938f	00		.
	add a,b			;9390	80		.
	nop			;9391	00		.
	nop			;9392	00		.
	ld bc,000c0h		;9393	01 c0 00	. . .
	nop			;9396	00		.
	inc bc			;9397	03		.
	ret po			;9398	e0		.
	nop			;9399	00		.
	nop			;939a	00		.
	ld bc,000c0h		;939b	01 c0 00	. . .
	nop			;939e	00		.
	ld bc,000c0h		;939f	01 c0 00	. . .
	nop			;93a2	00		.
	ld bc,000c0h		;93a3	01 c0 00	. . .
	nop			;93a6	00		.
	nop			;93a7	00		.
	nop			;93a8	00		.
	nop			;93a9	00		.
	nop			;93aa	00		.
	nop			;93ab	00		.
	nop			;93ac	00		.
	nop			;93ad	00		.
	ex af,af'		;93ae	08		.
	xor (hl)		;93af	ae		.
	xor 028h		;93b0	ee 28		. (
	ex af,af'		;93b2	08		.
	xor b			;93b3	a8		.
	ld (00e28h),hl		;93b4	22 28 0e	" ( .
	adc a,0eeh		;93b7	ce ee		. .
	jr c,l93c5h		;93b9	38 0a		8 .
	xor b			;93bb	a8		.
	adc a,b			;93bc	88		.
	jr z,l93cdh		;93bd	28 0e		( .
	xor 0eeh		;93bf	ee ee		. .
	jr c,l93c7h		;93c1	38 04		8 .
	dec c			;93c3	0d		.
	nop			;93c4	00		.
l93c5h:
	nop			;93c5	00		.
	add a,b			;93c6	80		.
l93c7h:
	nop			;93c7	00		.
	nop			;93c8	00		.
	ld bc,000c0h		;93c9	01 c0 00	. . .
	nop			;93cc	00		.
l93cdh:
	inc bc			;93cd	03		.
	ret po			;93ce	e0		.
	nop			;93cf	00		.
	nop			;93d0	00		.
	ld bc,000c0h		;93d1	01 c0 00	. . .
	nop			;93d4	00		.
	ld bc,000c0h		;93d5	01 c0 00	. . .
	nop			;93d8	00		.
	ld bc,000c0h		;93d9	01 c0 00	. . .
	nop			;93dc	00		.
	nop			;93dd	00		.
	nop			;93de	00		.
	nop			;93df	00		.
	nop			;93e0	00		.
	nop			;93e1	00		.
	nop			;93e2	00		.
	nop			;93e3	00		.
	ex af,af'		;93e4	08		.
	xor (hl)		;93e5	ae		.
	xor 038h		;93e6	ee 38		. 8
	ex af,af'		;93e8	08		.
	xor b			;93e9	a8		.
	ld (00e28h),hl		;93ea	22 28 0e	" ( .
	adc a,0eeh		;93ed	ce ee		. .
	jr c,l93fbh		;93ef	38 0a		8 .
	xor b			;93f1	a8		.
	adc a,b			;93f2	88		.
	jr z,$+16		;93f3	28 0e		( .
	xor 0eeh		;93f5	ee ee		. .
	jr c,$+35		;93f7	38 21		8 !
	nop			;93f9	00		.
	nop			;93fa	00		.
l93fbh:
	ld (l938ah),hl		;93fb	22 8a 93	" . .
	call sub_9231h		;93fe	cd 31 92	. 1 .
	ld hl,lbf00h		;9401	21 00 bf	! . .
	ld de,l922bh		;9404	11 2b 92	. + .
	call sub_b61ch		;9407	cd 1c b6	. . .
	ld hl,lb708h		;940a	21 08 b7	! . .
	ld de,l9387h		;940d	11 87 93	. . .
	call sub_b61ch		;9410	cd 1c b6	. . .
	ld hl,04718h		;9413	21 18 47	! . G
	ld de,l938ch		;9416	11 8c 93	. . .
	call sub_b5f8h		;9419	cd f8 b5	. . .
	ld hl,047c8h		;941c	21 c8 47	! . G
	ld de,093c2h		;941f	11 c2 93	. . .
	call sub_b5f8h		;9422	cd f8 b5	. . .
	ld hl,lb7eeh		;9425	21 ee b7	! . .
	ld de,09561h		;9428	11 61 95	. a .
	call sub_92a3h		;942b	cd a3 92	. . .
	ld hl,lb7f6h		;942e	21 f6 b7	! . .
	ld de,0956bh		;9431	11 6b 95	. k .
	call sub_92a3h		;9434	cd a3 92	. . .
	ld de,l954dh		;9437	11 4d 95	. M .
	ld b,00fh		;943a	06 0f		. .
	call sub_b796h		;943c	cd 96 b7	. . .
	ld hl,(l92bdh)		;943f	2a bd 92	* . .
	ld de,l92c1h		;9442	11 c1 92	. . .
	call sub_b5f8h		;9445	cd f8 b5	. . .
	ld hl,(l92bfh)		;9448	2a bf 92	* . .
	ld de,l9303h		;944b	11 03 93	. . .
	call sub_b5f8h		;944e	cd f8 b5	. . .
l9451h:
	ld hl,(l8d46h)		;9451	2a 46 8d	* F .
	inc hl			;9454	23		#
	ld (l8d46h),hl		;9455	22 46 8d	" F .
	bit 6,h			;9458	cb 74		. t
	jp nz,l927fh		;945a	c2 7f 92	. . .
	call sub_8eb4h		;945d	cd b4 8e	. . .
	ld de,(lb7e5h)		;9460	ed 5b e5 b7	. [ . .
	ld a,0f7h		;9464	3e f7		> .
	call sub_97a7h		;9466	cd a7 97	. . .
	bit 0,a			;9469	cb 47		. G
	jr z,l9478h		;946b	28 0b		( .
	ld a,e			;946d	7b		{
	and a			;946e	a7		.
	jr z,l94b1h		;946f	28 40		( @
	ld e,000h		;9471	1e 00		. .
	ld hl,l9571h		;9473	21 71 95	! q .
	jr l9494h		;9476	18 1c		. .
l9478h:
	bit 2,a			;9478	cb 57		. W
	jr z,l9488h		;947a	28 0c		( .
	ld a,e			;947c	7b		{
	cp 002h			;947d	fe 02		. .
	jr z,l94b1h		;947f	28 30		( 0
	ld e,002h		;9481	1e 02		. .
	ld hl,l9592h		;9483	21 92 95	! . .
	jr l9494h		;9486	18 0c		. .
l9488h:
	bit 1,a			;9488	cb 4f		. O
	jr z,l94b1h		;948a	28 25		( %
	dec e			;948c	1d		.
	jr z,l94b1h		;948d	28 22		( "
	ld e,001h		;948f	1e 01		. .
	ld hl,l9581h		;9491	21 81 95	! . .
l9494h:
	ld a,e			;9494	7b		{
	ld (lb7e5h),a		;9495	32 e5 b7	2 . .
	add a,a			;9498	87		.
	add a,a			;9499	87		.
	add a,a			;949a	87		.
	add a,a			;949b	87		.
	add a,02fh		;949c	c6 2f		. /
	ld e,a			;949e	5f		_
	ld d,000h		;949f	16 00		. .
	ld (l9618h),de		;94a1	ed 53 18 96	. S . .
	push hl			;94a5	e5		.
	ld de,(09614h)		;94a6	ed 5b 14 96	. [ . .
	call lb4ech		;94aa	cd ec b4	. . .
	pop hl			;94ad	e1		.
	ld (09614h),hl		;94ae	22 14 96	" . .
l94b1h:
	ld a,(l938ah)		;94b1	3a 8a 93	: . .
	and a			;94b4	a7		.
	jr z,l94bdh		;94b5	28 06		( .
	dec a			;94b7	3d		=
	ld (l938ah),a		;94b8	32 8a 93	2 . .
	jr l94f6h		;94bb	18 39		. 9
l94bdh:
	ld a,0fdh		;94bd	3e fd		> .
	call sub_97a7h		;94bf	cd a7 97	. . .
	rra			;94c2	1f		.
	jr nc,l94f6h		;94c3	30 31		0 1
	ld a,(lb7efh)		;94c5	3a ef b7	: . .
	inc a			;94c8	3c		<
	and 003h		;94c9	e6 03		. .
	ld (lb7efh),a		;94cb	32 ef b7	2 . .
	ld hl,(l92bdh)		;94ce	2a bd 92	* . .
	ld de,l9345h		;94d1	11 45 93	. E .
	call sub_b5f8h		;94d4	cd f8 b5	. . .
	ld a,(l92bdh+1)		;94d7	3a be 92	: . .
	add a,010h		;94da	c6 10		. .
	cp 0a0h			;94dc	fe a0		. .
	jr c,l94e2h		;94de	38 02		8 .
	ld a,06ch		;94e0	3e 6c		> l
l94e2h:
	ld (l92bdh+1),a		;94e2	32 be 92	2 . .
	ld hl,(l92bdh)		;94e5	2a bd 92	* . .
	ld de,l92c1h		;94e8	11 c1 92	. . .
	call sub_b5f8h		;94eb	cd f8 b5	. . .
	ld a,0ffh		;94ee	3e ff		> .
	ld (l938ah),a		;94f0	32 8a 93	2 . .
	call sub_c143h		;94f3	cd 43 c1	. C .
l94f6h:
	ld a,(l938bh)		;94f6	3a 8b 93	: . .
	and a			;94f9	a7		.
	jr z,l9502h		;94fa	28 06		( .
	dec a			;94fc	3d		=
	ld (l938bh),a		;94fd	32 8b 93	2 . .
	jr l953ch		;9500	18 3a		. :
l9502h:
	ld a,07fh		;9502	3e 7f		> .
	call sub_97a7h		;9504	cd a7 97	. . .
	and 010h		;9507	e6 10		. .
	jr z,l953ch		;9509	28 31		( 1
	ld a,(lb7f7h)		;950b	3a f7 b7	: . .
	inc a			;950e	3c		<
	and 003h		;950f	e6 03		. .
	ld (lb7f7h),a		;9511	32 f7 b7	2 . .
	ld hl,(l92bfh)		;9514	2a bf 92	* . .
	ld de,l9345h		;9517	11 45 93	. E .
	call sub_b5f8h		;951a	cd f8 b5	. . .
	ld a,(l92c0h)		;951d	3a c0 92	: . .
	add a,010h		;9520	c6 10		. .
	cp 0a0h			;9522	fe a0		. .
	jr c,l9528h		;9524	38 02		8 .
	ld a,06ch		;9526	3e 6c		> l
l9528h:
	ld (l92c0h),a		;9528	32 c0 92	2 . .
	ld hl,(l92bfh)		;952b	2a bf 92	* . .
	ld de,l9303h		;952e	11 03 93	. . .
	call sub_b5f8h		;9531	cd f8 b5	. . .
	ld a,0ffh		;9534	3e ff		> .
	ld (l938bh),a		;9536	32 8b 93	2 . .
	call sub_c143h		;9539	cd 43 c1	. C .
l953ch:
	ld de,l9618h		;953c	11 18 96	. . .
	call sub_961ch		;953f	cd 1c 96	. . .
	ld a,0efh		;9542	3e ef		> .
	call sub_97a7h		;9544	cd a7 97	. . .
	and 001h		;9547	e6 01		. .
	ret nz			;9549	c0		.
	jp l9451h		;954a	c3 51 94	. Q .
l954dh:
	jr $+17			;954d	18 0f		. .
	rlca			;954f	07		.
	inc b			;9550	04		.
	ld bc,01e26h		;9551	01 26 1e	. & .
	add hl,de		;9554	19		.
	ret z			;9555	c8		.
	rrca			;9556	0f		.
	rlca			;9557	07		.
	inc b			;9558	04		.
	ld (bc),a		;9559	02		.
	ld h,01eh		;955a	26 1e		& .
	add hl,de		;955c	19		.
	djnz $+25		;955d	10 17		. .
	rlca			;955f	07		.
	ld b,000h		;9560	06 00		. .
	nop			;9562	00		.
	nop			;9563	00		.
	nop			;9564	00		.
	nop			;9565	00		.
	nop			;9566	00		.
	ret nz			;9567	c0		.
	rla			;9568	17		.
	rlca			;9569	07		.
	ld b,000h		;956a	06 00		. .
	nop			;956c	00		.
	nop			;956d	00		.
	nop			;956e	00		.
	nop			;956f	00		.
	nop			;9570	00		.
l9571h:
	ld d,b			;9571	50		P
	cpl			;9572	2f		/
	ld b,h			;9573	44		D
	inc c			;9574	0c		.
	ld bc,02726h		;9575	01 26 27	. & '
	ld h,001h		;9578	26 01		& .
	ld h,019h		;957a	26 19		& .
	dec d			;957c	15		.
	ld a,(bc)		;957d	0a		.
	ld (01b0eh),hl		;957e	22 0e 1b	" . .
l9581h:
	ld d,b			;9581	50		P
	ccf			;9582	3f		?
	ld b,h			;9583	44		D
	dec c			;9584	0d		.
	ld (bc),a		;9585	02		.
	ld h,027h		;9586	26 27		& '
	ld h,002h		;9588	26 02		& .
	ld h,019h		;958a	26 19		& .
	dec d			;958c	15		.
	ld a,(bc)		;958d	0a		.
	ld (01b0eh),hl		;958e	22 0e 1b	" . .
	inc e			;9591	1c		.
l9592h:
	ld d,b			;9592	50		P
	ld c,a			;9593	4f		O
	ld b,h			;9594	44		D
	rrca			;9595	0f		.
	inc bc			;9596	03		.
	ld h,027h		;9597	26 27		& '
	ld h,00dh		;9599	26 0d		& .
	jr l95bbh		;959b	18 1e		. .
	dec bc			;959d	0b		.
	dec d			;959e	15		.
	ld c,026h		;959f	0e 26		. &
l95a1h:
	add hl,de		;95a1	19		.
	dec d			;95a2	15		.
	ld a,(bc)		;95a3	0a		.
	ld (0a750h),hl		;95a4	22 50 a7	" P .
	ld b,h			;95a7	44		D
	ld c,000h		;95a8	0e 00		. .
	ld h,027h		;95aa	26 27		& '
	ld h,01ch		;95ac	26 1c		& .
	dec e			;95ae	1d		.
	ld a,(bc)		;95af	0a		.
	dec de			;95b0	1b		.
	dec e			;95b1	1d		.
	ld h,010h		;95b2	26 10		& .
	ld a,(bc)		;95b4	0a		.
	ld d,00eh		;95b5	16 0e		. .
	ld h,b			;95b7	60		`
	ld h,a			;95b8	67		g
	ld b,l			;95b9	45		E
	ex af,af'		;95ba	08		.
l95bbh:
	inc d			;95bb	14		.
	ld c,022h		;95bc	0e 22		. "
	dec bc			;95be	0b		.
	jr l95cbh		;95bf	18 0a		. .
	dec de			;95c1	1b		.
	dec c			;95c2	0d		.
	ld h,b			;95c3	60		`
	ld (hl),a		;95c4	77		w
	ld b,l			;95c5	45		E
	ex af,af'		;95c6	08		.
	inc d			;95c7	14		.
	ld c,016h		;95c8	0e 16		. .
	add hl,de		;95ca	19		.
l95cbh:
	inc e			;95cb	1c		.
	dec e			;95cc	1d		.
	jr $+25			;95cd	18 17		. .
	ld l,b			;95cf	68		h
	add a,a			;95d0	87		.
	ld b,l			;95d1	45		E
	ld b,00ch		;95d2	06 0c		. .
	ld e,01bh		;95d4	1e 1b		. .
	inc e			;95d6	1c		.
	jr l95f4h		;95d7	18 1b		. .
	ld e,b			;95d9	58		X
	sub a			;95da	97		.
	ld b,l			;95db	45		E
	dec bc			;95dc	0b		.
	ld (de),a		;95dd	12		.
	rla			;95de	17		.
	dec e			;95df	1d		.
	ld c,01bh		;95e0	0e 1b		. .
	rrca			;95e2	0f		.
	ld a,(bc)		;95e3	0a		.
	inc c			;95e4	0c		.
	ld c,026h		;95e5	0e 26		. &
	add hl,hl		;95e7	29		)
	jr z,l95a1h		;95e8	28 b7		( .
	ld b,a			;95ea	47		G
	ld d,00ch		;95eb	16 0c		. .
	jr l9608h		;95ed	18 19		. .
	ld (0121bh),hl		;95ef	22 1b 12	" . .
	djnz l9605h		;95f2	10 11		. .
l95f4h:
	dec e			;95f4	1d		.
	ld h,011h		;95f5	26 11		& .
	ld (de),a		;95f7	12		.
	dec e			;95f8	1d		.
	ld h,019h		;95f9	26 19		& .
	ld a,(bc)		;95fb	0a		.
	inc d			;95fc	14		.
	ld h,001h		;95fd	26 01		& .
	add hl,bc		;95ff	09		.
	ex af,af'		;9600	08		.
	rlca			;9601	07		.
	ld (hl),b		;9602	70		p
	rrca			;9603	0f		.
	ld b,a			;9604	47		G
l9605h:
	dec b			;9605	05		.
	dec bc			;9606	0b		.
	ld a,(bc)		;9607	0a		.
l9608h:
	dec e			;9608	1d		.
	dec e			;9609	1d		.
	ld (01670h),hl		;960a	22 70 16	" p .
	ld b,(hl)		;960d	46		F
	dec b			;960e	05		.
	ld hl,(02a2ah)		;960f	2a 2a 2a	* * *
	ld hl,(l712ah)		;9612	2a 2a 71	* * q
	sub l			;9615	95		.
	or a			;9616	b7		.
	sub l			;9617	95		.
l9618h:
	cpl			;9618	2f		/
	nop			;9619	00		.
	ld h,a			;961a	67		g
	ex af,af'		;961b	08		.
sub_961ch:
	ld a,(de)		;961c	1a		.
	ld h,a			;961d	67		g
	ld l,070h		;961e	2e 70		. p
	call sub_b5a4h		;9620	cd a4 b5	. . .
	inc de			;9623	13		.
	ld a,(de)		;9624	1a		.
	push hl			;9625	e5		.
	ld hl,l9643h		;9626	21 43 96	! C .
	call sub_b5bbh		;9629	cd bb b5	. . .
	ld a,(hl)		;962c	7e		~
	pop hl			;962d	e1		.
	inc de			;962e	13		.
	ld b,00bh		;962f	06 0b		. .
l9631h:
	ld (hl),a		;9631	77		w
	inc l			;9632	2c		,
	djnz l9631h		;9633	10 fc		. .
	ld a,(l8d46h)		;9635	3a 46 8d	: F .
	and 01fh		;9638	e6 1f		. .
	ret nz			;963a	c0		.
	dec de			;963b	1b		.
	ld a,(de)		;963c	1a		.
	inc a			;963d	3c		<
	and 00fh		;963e	e6 0f		. .
	ld (de),a		;9640	12		.
	inc de			;9641	13		.
	ret			;9642	c9		.
l9643h:
	nop			;9643	00		.
	nop			;9644	00		.
	nop			;9645	00		.
	nop			;9646	00		.
	nop			;9647	00		.
	nop			;9648	00		.
	nop			;9649	00		.
	nop			;964a	00		.
	ld b,a			;964b	47		G
	ld b,a			;964c	47		G
	ld b,a			;964d	47		G
	ld b,a			;964e	47		G
	ld b,a			;964f	47		G
	ld b,a			;9650	47		G
	ld b,a			;9651	47		G
	ld b,a			;9652	47		G
l9653h:
	nop			;9653	00		.
l9654h:
	inc bc			;9654	03		.
	ld b,010h		;9655	06 10		. .
	dec d			;9657	15		.
	jr nz,$+39		;9658	20 25		  %
	ld d,b			;965a	50		P
	ld (hl),l		;965b	75		u
	rst 38h			;965c	ff		.
l965dh:
	ld a,(lb7e5h)		;965d	3a e5 b7	: . .
	cp 002h			;9660	fe 02		. .
	jr nz,l969bh		;9662	20 37		  7
	ld a,(l9653h)		;9664	3a 53 96	: S .
	and a			;9667	a7		.
	jr z,l969bh		;9668	28 31		( 1
	push bc			;966a	c5		.
	ld hl,lb7a6h		;966b	21 a6 b7	! . .
	ld de,lb7c8h		;966e	11 c8 b7	. . .
	ld b,00ah		;9671	06 0a		. .
	call sub_be02h		;9673	cd 02 be	. . .
	ld hl,lb7ech		;9676	21 ec b7	! . .
	ld de,lb7f4h		;9679	11 f4 b7	. . .
	ld b,003h		;967c	06 03		. .
	call sub_be02h		;967e	cd 02 be	. . .
	pop bc			;9681	c1		.
	call l969bh		;9682	cd 9b 96	. . .
	ld hl,lb7a6h		;9685	21 a6 b7	! . .
	ld de,lb7c8h		;9688	11 c8 b7	. . .
	ld b,00ah		;968b	06 0a		. .
	call sub_be02h		;968d	cd 02 be	. . .
	ld hl,lb7ech		;9690	21 ec b7	! . .
	ld de,lb7f4h		;9693	11 f4 b7	. . .
	ld b,003h		;9696	06 03		. .
	jp sub_be02h		;9698	c3 02 be	. . .
l969bh:
	ld hl,l9654h		;969b	21 54 96	! T .
	ld a,(lb7eeh)		;969e	3a ee b7	: . .
l96a1h:
	cp (hl)			;96a1	be		.
	jr c,l96a7h		;96a2	38 03		8 .
	inc hl			;96a4	23		#
	jr l96a1h		;96a5	18 fa		. .
l96a7h:
	ld e,(hl)		;96a7	5e		^
	ld hl,lb7ech		;96a8	21 ec b7	! . .
	ld a,c			;96ab	79		y
	add a,(hl)		;96ac	86		.
	daa			;96ad	27		'
	ld (hl),a		;96ae	77		w
	inc hl			;96af	23		#
	ld a,b			;96b0	78		x
	adc a,(hl)		;96b1	8e		.
	daa			;96b2	27		'
	ld (hl),a		;96b3	77		w
	inc hl			;96b4	23		#
	ld a,000h		;96b5	3e 00		> .
	adc a,(hl)		;96b7	8e		.
	daa			;96b8	27		'
	ld (hl),a		;96b9	77		w
	cp e			;96ba	bb		.
	jr c,l96f2h		;96bb	38 35		8 5
	push hl			;96bd	e5		.
	push ix			;96be	dd e5		. .
	ld ix,l9bc2h		;96c0	dd 21 c2 9b	. ! . .
	call sub_b684h		;96c4	cd 84 b6	. . .
	call sub_9910h		;96c7	cd 10 99	. . .
	call sub_9c25h		;96ca	cd 25 9c	. % .
	ld (ix+011h),000h	;96cd	dd 36 11 00	. 6 . .
	ld a,(ix+002h)		;96d1	dd 7e 02	. ~ .
	add a,010h		;96d4	c6 10		. .
	cp 0e9h			;96d6	fe e9		. .
	jr nc,l96ddh		;96d8	30 03		0 .
	ld (ix+002h),a		;96da	dd 77 02	. w .
l96ddh:
	ld a,(lb7e8h)		;96dd	3a e8 b7	: . .
	inc a			;96e0	3c		<
	ld (lb7e8h),a		;96e1	32 e8 b7	2 . .
	call sub_c064h		;96e4	cd 64 c0	. d .
	ld (ix+000h),007h	;96e7	dd 36 00 07	. 6 . .
	ld (ix+001h),020h	;96eb	dd 36 01 20	. 6 .  
	pop ix			;96ef	dd e1		. .
	pop hl			;96f1	e1		.
l96f2h:
	ld hl,(lb7a6h)		;96f2	2a a6 b7	* . .
	exx			;96f5	d9		.
	ld hl,lb7eeh		;96f6	21 ee b7	! . .
	ld a,001h		;96f9	3e 01		> .
	ld (05cddh),a		;96fb	32 dd 5c	2 . \
sub_96feh:
	ld b,003h		;96fe	06 03		. .
l9700h:
	ld a,(hl)		;9700	7e		~
	and 0f0h		;9701	e6 f0		. .
	call sub_9712h		;9703	cd 12 97	. . .
	ld a,(hl)		;9706	7e		~
	add a,a			;9707	87		.
	add a,a			;9708	87		.
	add a,a			;9709	87		.
	add a,a			;970a	87		.
	call sub_9712h		;970b	cd 12 97	. . .
	dec hl			;970e	2b		+
	djnz l9700h		;970f	10 ef		. .
	ret			;9711	c9		.
sub_9712h:
	ld de,l6975h		;9712	11 75 69	. u i
	add a,e			;9715	83		.
	ld e,a			;9716	5f		_
	jr nc,l971ah		;9717	30 01		0 .
	inc d			;9719	14		.
l971ah:
	ld (0972eh),de		;971a	ed 53 2e 97	. S . .
	exx			;971e	d9		.
	ld (09742h),hl		;971f	22 42 97	" B .
	ex de,hl		;9722	eb		.
	ld hl,01200h		;9723	21 00 12	! . .
	add hl,de		;9726	19		.
	ld a,008h		;9727	3e 08		> .
	ld (09746h),sp		;9729	ed 73 46 97	. s F .
	ld sp,00000h		;972d	31 00 00	1 . .
l9730h:
	ex af,af'		;9730	08		.
	pop bc			;9731	c1		.
	ld a,c			;9732	79		y
	or (hl)			;9733	b6		.
	xor b			;9734	a8		.
	ld (de),a		;9735	12		.
	ld bc,00020h		;9736	01 20 00	.   .
	add hl,bc		;9739	09		.
	ex de,hl		;973a	eb		.
	add hl,bc		;973b	09		.
	ex de,hl		;973c	eb		.
	ex af,af'		;973d	08		.
	dec a			;973e	3d		=
	jr nz,l9730h		;973f	20 ef		  .
	ld hl,00000h		;9741	21 00 00	! . .
	inc l			;9744	2c		,
	ld sp,00000h		;9745	31 00 00	1 . .
	exx			;9748	d9		.
	ret			;9749	c9		.
sub_974ah:
	xor a			;974a	af		.
	ld (05cddh),a		;974b	32 dd 5c	2 . \
	ld bc,00608h		;974e	01 08 06	. . .
	ld a,(lb7e5h)		;9751	3a e5 b7	: . .
	cp 002h			;9754	fe 02		. .
	jr nz,l9767h		;9756	20 0f		  .
	ld hl,01510h		;9758	21 10 15	! . .
	call sub_9cf4h		;975b	cd f4 9c	. . .
	ld bc,00608h		;975e	01 08 06	. . .
	ld hl,015c0h		;9761	21 c0 15	! . .
	jp sub_9cf4h		;9764	c3 f4 9c	. . .
l9767h:
	ld hl,01510h		;9767	21 10 15	! . .
	ld a,(lb7e6h)		;976a	3a e6 b7	: . .
	and a			;976d	a7		.
	jp z,sub_9cf4h		;976e	ca f4 9c	. . .
	ld l,0c0h		;9771	2e c0		. .
	jp sub_9cf4h		;9773	c3 f4 9c	. . .
sub_9776h:
	ld a,(lb7eah)		;9776	3a ea b7	: . .
sub_9779h:
	ld hl,l6cbdh		;9779	21 bd 6c	! . l
	add a,a			;977c	87		.
	ld e,a			;977d	5f		_
	ld d,000h		;977e	16 00		. .
	add hl,de		;9780	19		.
	ld e,(hl)		;9781	5e		^
	inc hl			;9782	23		#
	ld d,(hl)		;9783	56		V
	ex de,hl		;9784	eb		.
	ld (l9789h),hl		;9785	22 89 97	" . .
	ret			;9788	c9		.
l9789h:
	nop			;9789	00		.
	nop			;978a	00		.
sub_978bh:
	call sub_979fh		;978b	cd 9f 97	. . .
	ret z			;978e	c8		.
l978fh:
	call sub_979fh		;978f	cd 9f 97	. . .
	jr nz,l978fh		;9792	20 fb		  .
l9794h:
	call sub_979fh		;9794	cd 9f 97	. . .
	jr z,l9794h		;9797	28 fb		( .
l9799h:
	call sub_979fh		;9799	cd 9f 97	. . .
	jr nz,l9799h		;979c	20 fb		  .
	ret			;979e	c9		.
sub_979fh:
	ld a,0f7h		;979f	3e f7		> .
	call sub_97a7h		;97a1	cd a7 97	. . .
	and 00fh		;97a4	e6 0f		. .
	ret			;97a6	c9		.
sub_97a7h:
	in a,(0feh)		;97a7	db fe		. .
	cpl			;97a9	2f		/
	and 01fh		;97aa	e6 1f		. .
	ret			;97ac	c9		.
sub_97adh:
	ld (097d0h),sp		;97ad	ed 73 d0 97	. s . .
	ld sp,05b00h		;97b1	31 00 5b	1 . [
	ld bc,l8002h		;97b4	01 02 80	. . .
	ld de,00000h		;97b7	11 00 00	. . .
	jr l97c9h		;97ba	18 0d		. .
sub_97bch:
	ld (097d0h),sp		;97bc	ed 73 d0 97	. s . .
	ld sp,05800h		;97c0	31 00 58	1 . X
	ld bc,0000ch		;97c3	01 0c 00	. . .
	ld de,00000h		;97c6	11 00 00	. . .
l97c9h:
	push de			;97c9	d5		.
	djnz l97c9h		;97ca	10 fd		. .
	dec c			;97cc	0d		.
	jr nz,l97c9h		;97cd	20 fa		  .
	ld sp,00000h		;97cf	31 00 00	1 . .
	ret			;97d2	c9		.
sub_97d3h:
	ld e,0ffh		;97d3	1e ff		. .
l97d5h:
	dec e			;97d5	1d		.
	jr nz,l97d5h		;97d6	20 fd		  .
	dec d			;97d8	15		.
	jr nz,sub_97d3h		;97d9	20 f8		  .
	ret			;97db	c9		.
l97dch:
	nop			;97dc	00		.
	nop			;97dd	00		.
sub_97deh:
	ld ix,l9ad0h		;97de	dd 21 d0 9a	. ! . .
	ld b,00bh		;97e2	06 0b		. .
	xor a			;97e4	af		.
	ld (0d000h),a		;97e5	32 00 d0	2 . .
	ld hl,0d001h		;97e8	21 01 d0	! . .
l97ebh:
	push bc			;97eb	c5		.
	ld a,(ix+000h)		;97ec	dd 7e 00	. ~ .
	and a			;97ef	a7		.
	call nz,sub_9801h	;97f0	c4 01 98	. . .
	pop bc			;97f3	c1		.
	ld de,00016h		;97f4	11 16 00	. . .
	add ix,de		;97f7	dd 19		. .
	djnz l97ebh		;97f9	10 f0		. .
	ld (l97ffh),hl		;97fb	22 ff 97	" . .
	ret			;97fe	c9		.
l97ffh:
	nop			;97ff	00		.
	nop			;9800	00		.
sub_9801h:
	ld a,(ix+004h)		;9801	dd 7e 04	. ~ .
	cp 0c0h			;9804	fe c0		. .
	jr c,l980ch		;9806	38 04		8 .
	set 7,(ix+000h)		;9808	dd cb 00 fe	. . . .
l980ch:
	bit 7,(ix+000h)		;980c	dd cb 00 7e	. . . ~
	ret nz			;9810	c0		.
	ld a,(0d000h)		;9811	3a 00 d0	: . .
	inc a			;9814	3c		<
	ld (0d000h),a		;9815	32 00 d0	2 . .
	ld c,(ix+008h)		;9818	dd 4e 08	. N .
	ld a,(ix+002h)		;981b	dd 7e 02	. ~ .
	and 007h		;981e	e6 07		. .
	jr z,l9823h		;9820	28 01		( .
	inc c			;9822	0c		.
l9823h:
	ld a,c			;9823	79		y
	add a,a			;9824	87		.
	add a,a			;9825	87		.
	add a,a			;9826	87		.
	add a,(ix+002h)		;9827	dd 86 02	. . .
	jr nc,l9839h		;982a	30 0d		0 .
	ld a,0ffh		;982c	3e ff		> .
	sub (ix+002h)		;982e	dd 96 02	. . .
	srl a			;9831	cb 3f		. ?
	srl a			;9833	cb 3f		. ?
	srl a			;9835	cb 3f		. ?
	inc a			;9837	3c		<
	ld c,a			;9838	4f		O
l9839h:
	ld d,(ix+00ah)		;9839	dd 56 0a	. V .
	ld e,(ix+00bh)		;983c	dd 5e 0b	. ^ .
	ld (hl),e		;983f	73		s
	inc hl			;9840	23		#
	ld (hl),d		;9841	72		r
	inc hl			;9842	23		#
	sla c			;9843	cb 21		. !
	ld a,0f6h		;9845	3e f6		> .
	sub c			;9847	91		.
	ld (hl),a		;9848	77		w
	inc hl			;9849	23		#
	ld (l9876h+1),a		;984a	32 77 98	2 w .
	ld b,(ix+009h)		;984d	dd 46 09	. F .
	inc b			;9850	04		.
	ld (hl),b		;9851	70		p
	inc hl			;9852	23		#
	ex de,hl		;9853	eb		.
	ld a,l			;9854	7d		}
	ld c,0ffh		;9855	0e ff		. .
	jp l9876h		;9857	c3 76 98	. v .
	ldi			;985a	ed a0		. .
	ldi			;985c	ed a0		. .
	ldi			;985e	ed a0		. .
	ldi			;9860	ed a0		. .
	ldi			;9862	ed a0		. .
	ldi			;9864	ed a0		. .
	ldi			;9866	ed a0		. .
	ldi			;9868	ed a0		. .
	ldi			;986a	ed a0		. .
	ldi			;986c	ed a0		. .
	dec hl			;986e	2b		+
	add a,020h		;986f	c6 20		.  
	ld l,a			;9871	6f		o
	jp nc,l9876h		;9872	d2 76 98	. v .
	inc h			;9875	24		$
l9876h:
	djnz l9876h		;9876	10 fe		. .
	ex de,hl		;9878	eb		.
	ret			;9879	c9		.
sub_987ah:
	ld a,(l8e71h)		;987a	3a 71 8e	: q .
	and a			;987d	a7		.
	jr z,l989ah		;987e	28 1a		( .
	ld ix,(l8e70h)		;9880	dd 2a 70 8e	. * p .
	ld a,(ix+002h)		;9884	dd 7e 02	. ~ .
	sub 005h		;9887	d6 05		. .
	ld l,a			;9889	6f		o
	ld a,(ix+004h)		;988a	dd 7e 04	. ~ .
	sub 005h		;988d	d6 05		. .
	ld h,a			;988f	67		g
	ld bc,00417h		;9890	01 17 04	. . .
	call sub_9cf4h		;9893	cd f4 9c	. . .
	xor a			;9896	af		.
	ld (l8e71h),a		;9897	32 71 8e	2 q .
l989ah:
	ld a,(0d000h)		;989a	3a 00 d0	: . .
	and a			;989d	a7		.
	ret z			;989e	c8		.
	ld hl,0d001h		;989f	21 01 d0	! . .
l98a2h:
	ex af,af'		;98a2	08		.
	ld e,(hl)		;98a3	5e		^
	inc hl			;98a4	23		#
	ld d,(hl)		;98a5	56		V
	inc hl			;98a6	23		#
	ld a,(hl)		;98a7	7e		~
	ld (l98d5h+1),a		;98a8	32 d6 98	2 . .
	inc hl			;98ab	23		#
	ld b,(hl)		;98ac	46		F
	inc hl			;98ad	23		#
	ld c,0ffh		;98ae	0e ff		. .
	ld a,e			;98b0	7b		{
	jp l98d5h		;98b1	c3 d5 98	. . .
l98b4h:
	ex af,af'		;98b4	08		.
	dec a			;98b5	3d		=
	jr nz,l98a2h		;98b6	20 ea		  .
	ret			;98b8	c9		.
	ldi			;98b9	ed a0		. .
	ldi			;98bb	ed a0		. .
	ldi			;98bd	ed a0		. .
	ldi			;98bf	ed a0		. .
	ldi			;98c1	ed a0		. .
	ldi			;98c3	ed a0		. .
	ldi			;98c5	ed a0		. .
	ldi			;98c7	ed a0		. .
	ldi			;98c9	ed a0		. .
	ldi			;98cb	ed a0		. .
	dec de			;98cd	1b		.
	add a,020h		;98ce	c6 20		.  
	ld e,a			;98d0	5f		_
	jp nc,l98d5h		;98d1	d2 d5 98	. . .
	inc d			;98d4	14		.
l98d5h:
	djnz l98d5h		;98d5	10 fe		. .
	jp l98b4h		;98d7	c3 b4 98	. . .
	dec d			;98da	15		.
	sbc a,d			;98db	9a		.
	rrca			;98dc	0f		.
	sbc a,d			;98dd	9a		.
	add hl,bc		;98de	09		.
	sbc a,d			;98df	9a		.
	inc bc			;98e0	03		.
	sbc a,d			;98e1	9a		.
	defb 0fdh,099h,0f7h ;illegal sequence	;98e2	fd 99 f7	. . .
	sbc a,c			;98e5	99		.
	pop af			;98e6	f1		.
	sbc a,c			;98e7	99		.
	ex de,hl		;98e8	eb		.
	sbc a,c			;98e9	99		.
	sbc a,d			;98ea	9a		.
	sbc a,d			;98eb	9a		.
	adc a,h			;98ec	8c		.
	sbc a,d			;98ed	9a		.
	ld a,(hl)		;98ee	7e		~
	sbc a,d			;98ef	9a		.
	ld (hl),b		;98f0	70		p
	sbc a,d			;98f1	9a		.
	ld h,d			;98f2	62		b
	sbc a,d			;98f3	9a		.
	ld d,h			;98f4	54		T
	sbc a,d			;98f5	9a		.
	ld b,(hl)		;98f6	46		F
	sbc a,d			;98f7	9a		.
l98f8h:
	jr c,$-100		;98f8	38 9a		8 .
	ex af,af'		;98fa	08		.
	ld a,(bc)		;98fb	0a		.
	inc b			;98fc	04		.
	ld b,000h		;98fd	06 00		. .
	nop			;98ff	00		.
	ld b,000h		;9900	06 00		. .
	ld (bc),a		;9902	02		.
	ld (bc),a		;9903	02		.
	inc b			;9904	04		.
	inc b			;9905	04		.
	nop			;9906	00		.
	nop			;9907	00		.
	add hl,bc		;9908	09		.
	djnz $+8		;9909	10 06		. .
	inc c			;990b	0c		.
	inc b			;990c	04		.
	add hl,bc		;990d	09		.
	dec b			;990e	05		.
	rlca			;990f	07		.
sub_9910h:
	ld a,(ix+000h)		;9910	dd 7e 00	. ~ .
	bit 7,a			;9913	cb 7f		. .
	ret nz			;9915	c0		.
	cp 002h			;9916	fe 02		. .
	jr nz,l995ch		;9918	20 42		  B
	ld a,(l9b68h)		;991a	3a 68 9b	: h .
	cp 007h			;991d	fe 07		. .
	jr z,l9928h		;991f	28 07		( .
	ld a,(l9b52h)		;9921	3a 52 9b	: R .
	cp 007h			;9924	fe 07		. .
	jr nz,l9959h		;9926	20 31		  1
l9928h:
	ld (ix+001h),008h	;9928	dd 36 01 08	. 6 . .
	res 7,(ix+015h)		;992c	dd cb 15 be	. . . .
	ld a,(l8d46h)		;9930	3a 46 8d	: F .
	rra			;9933	1f		.
	jr c,l9959h		;9934	38 23		8 #
	ld a,(la65ah)		;9936	3a 5a a6	: Z .
	inc a			;9939	3c		<
	ld (la65ah),a		;993a	32 5a a6	2 Z .
	cp 0f8h			;993d	fe f8		. .
	jr c,l9959h		;993f	38 18		8 .
	ld a,(l9b68h)		;9941	3a 68 9b	: h .
	cp 007h			;9944	fe 07		. .
	jr nz,l994dh		;9946	20 05		  .
	ld a,0ffh		;9948	3e ff		> .
	ld (l9b68h),a		;994a	32 68 9b	2 h .
l994dh:
	ld a,(l9b52h)		;994d	3a 52 9b	: R .
	cp 007h			;9950	fe 07		. .
	jr nz,l9959h		;9952	20 05		  .
	ld a,0ffh		;9954	3e ff		> .
	ld (l9b52h),a		;9956	32 52 9b	2 R .
l9959h:
	ld a,(ix+000h)		;9959	dd 7e 00	. ~ .
l995ch:
	ld hl,l98f8h		;995c	21 f8 98	! . .
	add a,a			;995f	87		.
	call sub_b5bbh		;9960	cd bb b5	. . .
	ld a,(ix+002h)		;9963	dd 7e 02	. ~ .
	and 007h		;9966	e6 07		. .
	jr z,l996bh		;9968	28 01		( .
	inc hl			;996a	23		#
l996bh:
	ld a,(05cd8h)		;996b	3a d8 5c	: . \
	add a,(hl)		;996e	86		.
	ld (05cd8h),a		;996f	32 d8 5c	2 . \
	call sub_7767h		;9972	cd 67 77	. g w
	ld hl,098d8h		;9975	21 d8 98	! . .
	ld a,(ix+004h)		;9978	dd 7e 04	. ~ .
	cp 0c0h			;997b	fe c0		. .
	ret nc			;997d	d0		.
	add a,(ix+009h)		;997e	dd 86 09	. . .
	cp 020h			;9981	fe 20		.  
	ld (l97dch),sp		;9983	ed 73 dc 97	. s . .
	bit 7,(ix+015h)		;9987	dd cb 15 7e	. . . ~
	ld a,000h		;998b	3e 00		> .
	jr nz,l9992h		;998d	20 03		  .
	ld a,(ix+002h)		;998f	dd 7e 02	. ~ .
l9992h:
	and 007h		;9992	e6 07		. .
	ld c,a			;9994	4f		O
	ld a,(de)		;9995	1a		.
	ld b,a			;9996	47		G
	jr z,l999ch		;9997	28 03		( .
	add a,008h		;9999	c6 08		. .
	inc b			;999b	04		.
l999ch:
	add a,a			;999c	87		.
	call sub_b5bbh		;999d	cd bb b5	. . .
	ld a,(hl)		;99a0	7e		~
	inc hl			;99a1	23		#
	ld h,(hl)		;99a2	66		f
	ld l,a			;99a3	6f		o
	ld (l9a1fh+1),hl	;99a4	22 20 9a	"   .
	ld (l9ab3h+1),hl	;99a7	22 b4 9a	" . .
	ld (l9ab8h+1),hl	;99aa	22 b9 9a	" . .
	ld (l99e2h+1),hl	;99ad	22 e3 99	" . .
	inc de			;99b0	13		.
	ld a,021h		;99b1	3e 21		> !
	sub b			;99b3	90		.
	ld (09ab0h),a		;99b4	32 b0 9a	2 . .
	ld (09a1bh),a		;99b7	32 1b 9a	2 . .
	ld h,(ix+00ah)		;99ba	dd 66 0a	. f .
	ld l,(ix+00bh)		;99bd	dd 6e 0b	. n .
	ex de,hl		;99c0	eb		.
	ld a,(hl)		;99c1	7e		~
	ld b,a			;99c2	47		G
	ex af,af'		;99c3	08		.
	inc hl			;99c4	23		#
	ld sp,hl		;99c5	f9		.
	ld a,(ix+004h)		;99c6	dd 7e 04	. ~ .
	add a,b			;99c9	80		.
	cp 0c1h			;99ca	fe c1		. .
	jr c,l99d5h		;99cc	38 07		8 .
	ld a,0c0h		;99ce	3e c0		> .
	sub (ix+004h)		;99d0	dd 96 04	. . .
	ld b,a			;99d3	47		G
	ex af,af'		;99d4	08		.
l99d5h:
	ld a,c			;99d5	79		y
	and a			;99d6	a7		.
	jr z,l99e5h		;99d7	28 0c		( .
	add a,a			;99d9	87		.
	add a,0f0h		;99da	c6 f0		. .
	ld h,a			;99dc	67		g
	ld (09ac2h),de		;99dd	ed 53 c2 9a	. S . .
	ld a,(de)		;99e1	1a		.
l99e2h:
	jp l99e2h		;99e2	c3 e2 99	. . .
l99e5h:
	ex de,hl		;99e5	eb		.
	ld (09a28h),hl		;99e6	22 28 9a	" ( .
	jr l99e2h		;99e9	18 f7		. .
	pop de			;99eb	d1		.
	ld a,e			;99ec	7b		{
	or (hl)			;99ed	b6		.
	xor d			;99ee	aa		.
	ld (hl),a		;99ef	77		w
	inc l			;99f0	2c		,
	pop de			;99f1	d1		.
	ld a,e			;99f2	7b		{
	or (hl)			;99f3	b6		.
	xor d			;99f4	aa		.
	ld (hl),a		;99f5	77		w
	inc l			;99f6	2c		,
	pop de			;99f7	d1		.
	ld a,e			;99f8	7b		{
	or (hl)			;99f9	b6		.
	xor d			;99fa	aa		.
	ld (hl),a		;99fb	77		w
	inc l			;99fc	2c		,
	pop de			;99fd	d1		.
	ld a,e			;99fe	7b		{
l99ffh:
	or (hl)			;99ff	b6		.
	xor d			;9a00	aa		.
	ld (hl),a		;9a01	77		w
	inc l			;9a02	2c		,
	pop de			;9a03	d1		.
	ld a,e			;9a04	7b		{
	or (hl)			;9a05	b6		.
	xor d			;9a06	aa		.
	ld (hl),a		;9a07	77		w
	inc l			;9a08	2c		,
	pop de			;9a09	d1		.
	ld a,e			;9a0a	7b		{
	or (hl)			;9a0b	b6		.
	xor d			;9a0c	aa		.
	ld (hl),a		;9a0d	77		w
	inc l			;9a0e	2c		,
	pop de			;9a0f	d1		.
	ld a,e			;9a10	7b		{
	or (hl)			;9a11	b6		.
	xor d			;9a12	aa		.
	ld (hl),a		;9a13	77		w
	inc l			;9a14	2c		,
	pop de			;9a15	d1		.
	ld a,e			;9a16	7b		{
	or (hl)			;9a17	b6		.
	xor d			;9a18	aa		.
	ld (hl),a		;9a19	77		w
	ld de,00000h		;9a1a	11 00 00	. . .
	add hl,de		;9a1d	19		.
	dec b			;9a1e	05		.
l9a1fh:
	jp nz,l9a1fh		;9a1f	c2 1f 9a	. . .
l9a22h:
	ld sp,(l97dch)		;9a22	ed 7b dc 97	. { . .
	ret			;9a26	c9		.
	ld hl,00000h		;9a27	21 00 00	! . .
	ld de,00020h		;9a2a	11 20 00	.   .
	add hl,de		;9a2d	19		.
	ld (09a28h),hl		;9a2e	22 28 9a	" ( .
	dec b			;9a31	05		.
	jp nz,l9ab8h		;9a32	c2 b8 9a	. . .
	jp l9a22h		;9a35	c3 22 9a	. " .
	pop bc			;9a38	c1		.
	ld l,c			;9a39	69		i
	or (hl)			;9a3a	b6		.
	ld l,b			;9a3b	68		h
	xor (hl)		;9a3c	ae		.
	ld (de),a		;9a3d	12		.
	inc e			;9a3e	1c		.
	inc h			;9a3f	24		$
	ld l,c			;9a40	69		i
	ld a,(de)		;9a41	1a		.
	or (hl)			;9a42	b6		.
	ld l,b			;9a43	68		h
	xor (hl)		;9a44	ae		.
	dec h			;9a45	25		%
	pop bc			;9a46	c1		.
	ld l,c			;9a47	69		i
	or (hl)			;9a48	b6		.
	ld l,b			;9a49	68		h
	xor (hl)		;9a4a	ae		.
	ld (de),a		;9a4b	12		.
	inc e			;9a4c	1c		.
	inc h			;9a4d	24		$
	ld l,c			;9a4e	69		i
	ld a,(de)		;9a4f	1a		.
	or (hl)			;9a50	b6		.
	ld l,b			;9a51	68		h
	xor (hl)		;9a52	ae		.
	dec h			;9a53	25		%
	pop bc			;9a54	c1		.
	ld l,c			;9a55	69		i
	or (hl)			;9a56	b6		.
	ld l,b			;9a57	68		h
	xor (hl)		;9a58	ae		.
	ld (de),a		;9a59	12		.
	inc e			;9a5a	1c		.
	inc h			;9a5b	24		$
	ld l,c			;9a5c	69		i
	ld a,(de)		;9a5d	1a		.
	or (hl)			;9a5e	b6		.
	ld l,b			;9a5f	68		h
	xor (hl)		;9a60	ae		.
	dec h			;9a61	25		%
	pop bc			;9a62	c1		.
	ld l,c			;9a63	69		i
	or (hl)			;9a64	b6		.
	ld l,b			;9a65	68		h
	xor (hl)		;9a66	ae		.
	ld (de),a		;9a67	12		.
	inc e			;9a68	1c		.
	inc h			;9a69	24		$
	ld l,c			;9a6a	69		i
	ld a,(de)		;9a6b	1a		.
	or (hl)			;9a6c	b6		.
	ld l,b			;9a6d	68		h
	xor (hl)		;9a6e	ae		.
	dec h			;9a6f	25		%
	pop bc			;9a70	c1		.
	ld l,c			;9a71	69		i
	or (hl)			;9a72	b6		.
	ld l,b			;9a73	68		h
	xor (hl)		;9a74	ae		.
	ld (de),a		;9a75	12		.
	inc e			;9a76	1c		.
	inc h			;9a77	24		$
	ld l,c			;9a78	69		i
	ld a,(de)		;9a79	1a		.
	or (hl)			;9a7a	b6		.
	ld l,b			;9a7b	68		h
	xor (hl)		;9a7c	ae		.
	dec h			;9a7d	25		%
	pop bc			;9a7e	c1		.
	ld l,c			;9a7f	69		i
	or (hl)			;9a80	b6		.
	ld l,b			;9a81	68		h
	xor (hl)		;9a82	ae		.
	ld (de),a		;9a83	12		.
	inc e			;9a84	1c		.
	inc h			;9a85	24		$
	ld l,c			;9a86	69		i
	ld a,(de)		;9a87	1a		.
	or (hl)			;9a88	b6		.
	ld l,b			;9a89	68		h
	xor (hl)		;9a8a	ae		.
	dec h			;9a8b	25		%
	pop bc			;9a8c	c1		.
	ld l,c			;9a8d	69		i
	or (hl)			;9a8e	b6		.
	ld l,b			;9a8f	68		h
	xor (hl)		;9a90	ae		.
	ld (de),a		;9a91	12		.
	inc e			;9a92	1c		.
	inc h			;9a93	24		$
	ld l,c			;9a94	69		i
	ld a,(de)		;9a95	1a		.
	or (hl)			;9a96	b6		.
	ld l,b			;9a97	68		h
	xor (hl)		;9a98	ae		.
	dec h			;9a99	25		%
	pop bc			;9a9a	c1		.
	ld l,c			;9a9b	69		i
	or (hl)			;9a9c	b6		.
	ld l,b			;9a9d	68		h
	xor (hl)		;9a9e	ae		.
	ld (de),a		;9a9f	12		.
	inc e			;9aa0	1c		.
	inc h			;9aa1	24		$
	ld l,c			;9aa2	69		i
	ld a,(de)		;9aa3	1a		.
	or (hl)			;9aa4	b6		.
	ld l,b			;9aa5	68		h
	xor (hl)		;9aa6	ae		.
	dec h			;9aa7	25		%
	ld (de),a		;9aa8	12		.
	ex af,af'		;9aa9	08		.
	dec a			;9aaa	3d		=
	jr z,l9abbh		;9aab	28 0e		( .
	ex af,af'		;9aad	08		.
	ld a,e			;9aae	7b		{
	add a,000h		;9aaf	c6 00		. .
	ld e,a			;9ab1	5f		_
	ld a,(de)		;9ab2	1a		.
l9ab3h:
	jp nc,l9ab3h		;9ab3	d2 b3 9a	. . .
	inc d			;9ab6	14		.
l9ab7h:
	ld a,(de)		;9ab7	1a		.
l9ab8h:
	jp l9ab8h		;9ab8	c3 b8 9a	. . .
l9abbh:
	ld sp,(l97dch)		;9abb	ed 7b dc 97	. { . .
	ret			;9abf	c9		.
	ld a,h			;9ac0	7c		|
	ld hl,00000h		;9ac1	21 00 00	! . .
	ld de,00020h		;9ac4	11 20 00	.   .
	add hl,de		;9ac7	19		.
	ld (09ac2h),hl		;9ac8	22 c2 9a	" . .
	ex de,hl		;9acb	eb		.
	ld h,a			;9acc	67		g
	jp l9ab7h		;9acd	c3 b7 9a	. . .
l9ad0h:
	ld (bc),a		;9ad0	02		.
	nop			;9ad1	00		.
l9ad2h:
	add a,h			;9ad2	84		.
	nop			;9ad3	00		.
	and b			;9ad4	a0		.
	nop			;9ad5	00		.
l9ad6h:
	jr c,l9adah		;9ad6	38 02		8 .
	ld (bc),a		;9ad8	02		.
	inc c			;9ad9	0c		.
l9adah:
	nop			;9ada	00		.
	nop			;9adb	00		.
	ex af,af'		;9adc	08		.
	rlca			;9add	07		.
	nop			;9ade	00		.
	nop			;9adf	00		.
	nop			;9ae0	00		.
	nop			;9ae1	00		.
l9ae2h:
	nop			;9ae2	00		.
l9ae3h:
	nop			;9ae3	00		.
l9ae4h:
	nop			;9ae4	00		.
	add a,b			;9ae5	80		.
l9ae6h:
	nop			;9ae6	00		.
	nop			;9ae7	00		.
	add a,h			;9ae8	84		.
	nop			;9ae9	00		.
	and b			;9aea	a0		.
	nop			;9aeb	00		.
	jr c,l9af0h		;9aec	38 02		8 .
	ld (bc),a		;9aee	02		.
	inc c			;9aef	0c		.
l9af0h:
	nop			;9af0	00		.
	nop			;9af1	00		.
	ex af,af'		;9af2	08		.
	rlca			;9af3	07		.
	nop			;9af4	00		.
	nop			;9af5	00		.
	nop			;9af6	00		.
	nop			;9af7	00		.
	nop			;9af8	00		.
l9af9h:
	nop			;9af9	00		.
	nop			;9afa	00		.
	add a,b			;9afb	80		.
l9afch:
	nop			;9afc	00		.
	nop			;9afd	00		.
	add a,h			;9afe	84		.
	nop			;9aff	00		.
	and b			;9b00	a0		.
	nop			;9b01	00		.
	jr c,l9b06h		;9b02	38 02		8 .
	ld (bc),a		;9b04	02		.
	inc c			;9b05	0c		.
l9b06h:
	nop			;9b06	00		.
	nop			;9b07	00		.
	ex af,af'		;9b08	08		.
	rlca			;9b09	07		.
	nop			;9b0a	00		.
	nop			;9b0b	00		.
	nop			;9b0c	00		.
	nop			;9b0d	00		.
	nop			;9b0e	00		.
l9b0fh:
	nop			;9b0f	00		.
	nop			;9b10	00		.
	add a,b			;9b11	80		.
l9b12h:
	nop			;9b12	00		.
	nop			;9b13	00		.
	add a,h			;9b14	84		.
	nop			;9b15	00		.
	and b			;9b16	a0		.
	nop			;9b17	00		.
	jr nc,$+3		;9b18	30 01		0 .
	ld bc,00008h		;9b1a	01 08 00	. . .
	nop			;9b1d	00		.
	inc b			;9b1e	04		.
	ex af,af'		;9b1f	08		.
	nop			;9b20	00		.
	nop			;9b21	00		.
	nop			;9b22	00		.
	nop			;9b23	00		.
	nop			;9b24	00		.
	nop			;9b25	00		.
	nop			;9b26	00		.
	add a,b			;9b27	80		.
l9b28h:
	nop			;9b28	00		.
	nop			;9b29	00		.
	add a,h			;9b2a	84		.
	nop			;9b2b	00		.
	and b			;9b2c	a0		.
	nop			;9b2d	00		.
	jr nc,$+3		;9b2e	30 01		0 .
	ld bc,00008h		;9b30	01 08 00	. . .
	nop			;9b33	00		.
	inc b			;9b34	04		.
	ex af,af'		;9b35	08		.
	nop			;9b36	00		.
	nop			;9b37	00		.
	nop			;9b38	00		.
	nop			;9b39	00		.
	nop			;9b3a	00		.
	nop			;9b3b	00		.
	nop			;9b3c	00		.
	add a,b			;9b3d	80		.
l9b3eh:
	nop			;9b3e	00		.
	nop			;9b3f	00		.
l9b40h:
	ld (hl),h		;9b40	74		t
	nop			;9b41	00		.
l9b42h:
	xor l			;9b42	ad		.
	nop			;9b43	00		.
	nop			;9b44	00		.
	nop			;9b45	00		.
	inc b			;9b46	04		.
	dec c			;9b47	0d		.
	nop			;9b48	00		.
	nop			;9b49	00		.
	inc e			;9b4a	1c		.
	ld a,(bc)		;9b4b	0a		.
	nop			;9b4c	00		.
	nop			;9b4d	00		.
	nop			;9b4e	00		.
	nop			;9b4f	00		.
	ret p			;9b50	f0		.
	nop			;9b51	00		.
l9b52h:
	rst 38h			;9b52	ff		.
	add a,b			;9b53	80		.
l9b54h:
	ld bc,l7400h		;9b54	01 00 74	. . t
	nop			;9b57	00		.
l9b58h:
	xor l			;9b58	ad		.
	nop			;9b59	00		.
	nop			;9b5a	00		.
	nop			;9b5b	00		.
	inc b			;9b5c	04		.
	dec c			;9b5d	0d		.
	nop			;9b5e	00		.
	nop			;9b5f	00		.
l9b60h:
	inc e			;9b60	1c		.
	ld a,(bc)		;9b61	0a		.
	nop			;9b62	00		.
	nop			;9b63	00		.
	nop			;9b64	00		.
	nop			;9b65	00		.
	ret p			;9b66	f0		.
	nop			;9b67	00		.
l9b68h:
	nop			;9b68	00		.
	add a,b			;9b69	80		.
l9b6ah:
	nop			;9b6a	00		.
	inc bc			;9b6b	03		.
l9b6ch:
	add a,h			;9b6c	84		.
	nop			;9b6d	00		.
	xor l			;9b6e	ad		.
	nop			;9b6f	00		.
l9b70h:
	nop			;9b70	00		.
	nop			;9b71	00		.
	inc bc			;9b72	03		.
	dec c			;9b73	0d		.
	nop			;9b74	00		.
	nop			;9b75	00		.
	dec de			;9b76	1b		.
	ld a,(bc)		;9b77	0a		.
	nop			;9b78	00		.
	nop			;9b79	00		.
	nop			;9b7a	00		.
l9b7bh:
	nop			;9b7b	00		.
	nop			;9b7c	00		.
	nop			;9b7d	00		.
	nop			;9b7e	00		.
	nop			;9b7f	00		.
l9b80h:
	nop			;9b80	00		.
l9b81h:
	nop			;9b81	00		.
l9b82h:
	jr z,l9b84h		;9b82	28 00		( .
l9b84h:
	sbc a,a			;9b84	9f		.
	nop			;9b85	00		.
	nop			;9b86	00		.
	nop			;9b87	00		.
l9b88h:
	nop			;9b88	00		.
	nop			;9b89	00		.
	nop			;9b8a	00		.
	nop			;9b8b	00		.
l9b8ch:
	nop			;9b8c	00		.
l9b8dh:
	nop			;9b8d	00		.
	nop			;9b8e	00		.
	nop			;9b8f	00		.
	nop			;9b90	00		.
l9b91h:
	nop			;9b91	00		.
	ret p			;9b92	f0		.
	ld h,b			;9b93	60		`
	nop			;9b94	00		.
	nop			;9b95	00		.
l9b96h:
	nop			;9b96	00		.
	ld bc,00078h		;9b97	01 78 00	. x .
	adc a,b			;9b9a	88		.
	nop			;9b9b	00		.
	nop			;9b9c	00		.
	nop			;9b9d	00		.
	inc bc			;9b9e	03		.
	jr l9ba1h		;9b9f	18 00		. .
l9ba1h:
	nop			;9ba1	00		.
	jr l9bbch		;9ba2	18 18		. .
	nop			;9ba4	00		.
	nop			;9ba5	00		.
	nop			;9ba6	00		.
	nop			;9ba7	00		.
	ld d,b			;9ba8	50		P
	ld b,h			;9ba9	44		D
	nop			;9baa	00		.
	nop			;9bab	00		.
l9bach:
	nop			;9bac	00		.
	nop			;9bad	00		.
l9baeh:
	ret m			;9bae	f8		.
	nop			;9baf	00		.
l9bb0h:
	xor b			;9bb0	a8		.
	nop			;9bb1	00		.
	nop			;9bb2	00		.
	nop			;9bb3	00		.
	inc bc			;9bb4	03		.
	inc e			;9bb5	1c		.
	nop			;9bb6	00		.
	nop			;9bb7	00		.
	nop			;9bb8	00		.
	nop			;9bb9	00		.
	nop			;9bba	00		.
	nop			;9bbb	00		.
l9bbch:
	nop			;9bbc	00		.
l9bbdh:
	nop			;9bbd	00		.
	nop			;9bbe	00		.
	nop			;9bbf	00		.
	nop			;9bc0	00		.
	nop			;9bc1	00		.
l9bc2h:
	inc bc			;9bc2	03		.
	nop			;9bc3	00		.
l9bc4h:
	djnz l9bc6h		;9bc4	10 00		. .
l9bc6h:
	cp c			;9bc6	b9		.
	nop			;9bc7	00		.
	nop			;9bc8	00		.
	nop			;9bc9	00		.
	ld (bc),a		;9bca	02		.
	ld b,000h		;9bcb	06 00		. .
	nop			;9bcd	00		.
	nop			;9bce	00		.
	nop			;9bcf	00		.
	nop			;9bd0	00		.
	nop			;9bd1	00		.
	nop			;9bd2	00		.
	nop			;9bd3	00		.
l9bd4h:
	inc bc			;9bd4	03		.
	nop			;9bd5	00		.
	nop			;9bd6	00		.
	nop			;9bd7	00		.
	inc c			;9bd8	0c		.
	nop			;9bd9	00		.
	nop			;9bda	00		.
	nop			;9bdb	00		.
	nop			;9bdc	00		.
	nop			;9bdd	00		.
	nop			;9bde	00		.
	nop			;9bdf	00		.
	nop			;9be0	00		.
	nop			;9be1	00		.
	nop			;9be2	00		.
	nop			;9be3	00		.
	nop			;9be4	00		.
	nop			;9be5	00		.
	nop			;9be6	00		.
	nop			;9be7	00		.
	nop			;9be8	00		.
	nop			;9be9	00		.
	nop			;9bea	00		.
	nop			;9beb	00		.
	nop			;9bec	00		.
	nop			;9bed	00		.
l9beeh:
	inc bc			;9bee	03		.
	dec b			;9bef	05		.
	ld a,l			;9bf0	7d		}
	nop			;9bf1	00		.
	xor c			;9bf2	a9		.
	nop			;9bf3	00		.
	nop			;9bf4	00		.
	nop			;9bf5	00		.
	nop			;9bf6	00		.
	nop			;9bf7	00		.
	nop			;9bf8	00		.
	nop			;9bf9	00		.
	nop			;9bfa	00		.
	nop			;9bfb	00		.
	nop			;9bfc	00		.
	nop			;9bfd	00		.
	nop			;9bfe	00		.
	nop			;9bff	00		.
	nop			;9c00	00		.
	nop			;9c01	00		.
	nop			;9c02	00		.
	nop			;9c03	00		.
	nop			;9c04	00		.
	nop			;9c05	00		.
	nop			;9c06	00		.
	nop			;9c07	00		.
l9c08h:
	nop			;9c08	00		.
	nop			;9c09	00		.
	nop			;9c0a	00		.
	nop			;9c0b	00		.
	nop			;9c0c	00		.
	nop			;9c0d	00		.
	nop			;9c0e	00		.
	nop			;9c0f	00		.
	nop			;9c10	00		.
	nop			;9c11	00		.
	nop			;9c12	00		.
	nop			;9c13	00		.
	nop			;9c14	00		.
	nop			;9c15	00		.
	nop			;9c16	00		.
	nop			;9c17	00		.
	nop			;9c18	00		.
	nop			;9c19	00		.
	nop			;9c1a	00		.
	nop			;9c1b	00		.
	nop			;9c1c	00		.
	nop			;9c1d	00		.
	nop			;9c1e	00		.
	nop			;9c1f	00		.
	nop			;9c20	00		.
	nop			;9c21	00		.
	nop			;9c22	00		.
	nop			;9c23	00		.
l9c24h:
	nop			;9c24	00		.
sub_9c25h:
	ld a,(ix+000h)		;9c25	dd 7e 00	. ~ .
	rla			;9c28	17		.
	jr nc,l9c2fh		;9c29	30 04		0 .
	ld (ix+000h),000h	;9c2b	dd 36 00 00	. 6 . .
l9c2fh:
	ld l,(ix+002h)		;9c2f	dd 6e 02	. n .
	ld a,(ix+011h)		;9c32	dd 7e 11	. ~ .
	and a			;9c35	a7		.
	jr nz,l9c49h		;9c36	20 11		  .
	ld h,(ix+004h)		;9c38	dd 66 04	. f .
	ld a,(ix+008h)		;9c3b	dd 7e 08	. ~ .
	add a,a			;9c3e	87		.
	add a,a			;9c3f	87		.
	add a,a			;9c40	87		.
	add a,l			;9c41	85		.
	ld b,a			;9c42	47		G
	ld c,(ix+009h)		;9c43	dd 4e 09	. N .
	jp l9c88h		;9c46	c3 88 9c	. . .
l9c49h:
	ld e,l			;9c49	5d		]
	ld a,(ix+00eh)		;9c4a	dd 7e 0e	. ~ .
	ld d,a			;9c4d	57		W
	cp l			;9c4e	bd		.
	jr nc,l9c52h		;9c4f	30 01		0 .
	ld l,a			;9c51	6f		o
l9c52h:
	ld b,(ix+008h)		;9c52	dd 46 08	. F .
	sla b			;9c55	cb 20		.  
	sla b			;9c57	cb 20		.  
	sla b			;9c59	cb 20		.  
	ld c,(ix+010h)		;9c5b	dd 4e 10	. N .
	sla c			;9c5e	cb 21		. !
	sla c			;9c60	cb 21		. !
	sla c			;9c62	cb 21		. !
	ld a,e			;9c64	7b		{
	add a,b			;9c65	80		.
	ld b,a			;9c66	47		G
	ld a,d			;9c67	7a		z
	add a,c			;9c68	81		.
	cp b			;9c69	b8		.
	jr c,l9c6dh		;9c6a	38 01		8 .
	ld b,a			;9c6c	47		G
l9c6dh:
	ld h,(ix+004h)		;9c6d	dd 66 04	. f .
	ld d,h			;9c70	54		T
	ld a,(ix+00fh)		;9c71	dd 7e 0f	. ~ .
	ld e,a			;9c74	5f		_
	cp h			;9c75	bc		.
	jr nc,l9c79h		;9c76	30 01		0 .
	ld h,a			;9c78	67		g
l9c79h:
	ld a,d			;9c79	7a		z
	add a,(ix+009h)		;9c7a	dd 86 09	. . .
	ld d,a			;9c7d	57		W
	ld a,e			;9c7e	7b		{
	add a,(ix+011h)		;9c7f	dd 86 11	. . .
	cp d			;9c82	ba		.
	jr nc,l9c86h		;9c83	30 01		0 .
	ld a,d			;9c85	7a		z
l9c86h:
	sub h			;9c86	94		.
	ld c,a			;9c87	4f		O
l9c88h:
	ld a,l			;9c88	7d		}
	and 0f8h		;9c89	e6 f8		. .
	ld l,a			;9c8b	6f		o
	cp 0f8h			;9c8c	fe f8		. .
	ret nc			;9c8e	d0		.
	cp b			;9c8f	b8		.
	jr c,l9c94h		;9c90	38 02		8 .
	ld b,0ffh		;9c92	06 ff		. .
l9c94h:
	ld e,l			;9c94	5d		]
	bit 7,a			;9c95	cb 7f		. .
	jr z,l9c9dh		;9c97	28 04		( .
	res 7,l			;9c99	cb bd		. .
	res 7,b			;9c9b	cb b8		. .
l9c9dh:
	ld a,b			;9c9d	78		x
	add a,007h		;9c9e	c6 07		. .
	and 0f8h		;9ca0	e6 f8		. .
	sub l			;9ca2	95		.
	srl a			;9ca3	cb 3f		. ?
	srl a			;9ca5	cb 3f		. ?
	srl a			;9ca7	cb 3f		. ?
	ld b,a			;9ca9	47		G
	ld l,e			;9caa	6b		k
	ld a,(ix+002h)		;9cab	dd 7e 02	. ~ .
	ld (ix+00eh),a		;9cae	dd 77 0e	. w .
	ld a,(ix+004h)		;9cb1	dd 7e 04	. ~ .
	ld (ix+00fh),a		;9cb4	dd 77 0f	. w .
	ld a,(ix+008h)		;9cb7	dd 7e 08	. ~ .
	ld (ix+010h),a		;9cba	dd 77 10	. w .
	ld a,(ix+009h)		;9cbd	dd 7e 09	. ~ .
	ld (ix+011h),a		;9cc0	dd 77 11	. w .
	ld a,h			;9cc3	7c		|
	add a,c			;9cc4	81		.
	cp 0c0h			;9cc5	fe c0		. .
	jr c,l9ccdh		;9cc7	38 04		8 .
	ld a,0c0h		;9cc9	3e c0		> .
	sub h			;9ccb	94		.
	ld c,a			;9ccc	4f		O
l9ccdh:
	bit 7,l			;9ccd	cb 7d		. }
	jr z,l9ce8h		;9ccf	28 17		( .
	res 7,l			;9cd1	cb bd		. .
	ld a,b			;9cd3	78		x
	add a,a			;9cd4	87		.
	add a,a			;9cd5	87		.
	add a,a			;9cd6	87		.
	add a,l			;9cd7	85		.
	sub 078h		;9cd8	d6 78		. x
	jr c,l9ce6h		;9cda	38 0a		8 .
	srl a			;9cdc	cb 3f		. ?
	srl a			;9cde	cb 3f		. ?
	srl a			;9ce0	cb 3f		. ?
	neg			;9ce2	ed 44		. D
	add a,b			;9ce4	80		.
	ld b,a			;9ce5	47		G
l9ce6h:
	set 7,l			;9ce6	cb fd		. .
l9ce8h:
	ld a,h			;9ce8	7c		|
	sub 008h		;9ce9	d6 08		. .
	jr nc,sub_9cf4h		;9ceb	30 07		0 .
	add a,c			;9ced	81		.
	ld c,a			;9cee	4f		O
	dec a			;9cef	3d		=
	rla			;9cf0	17		.
	ret c			;9cf1	d8		.
	ld h,008h		;9cf2	26 08		& .
sub_9cf4h:
	push bc			;9cf4	c5		.
	push hl			;9cf5	e5		.
	call sub_c03dh		;9cf6	cd 3d c0	. = .
	ex de,hl		;9cf9	eb		.
	pop hl			;9cfa	e1		.
	call sub_b57dh		;9cfb	cd 7d b5	. } .
	ex de,hl		;9cfe	eb		.
l9cffh:
	pop bc			;9cff	c1		.
	ld a,b			;9d00	78		x
	exx			;9d01	d9		.
	ld c,a			;9d02	4f		O
	ld a,020h		;9d03	3e 20		>  
	sub c			;9d05	91		.
	ld (l9d3ah+1),a		;9d06	32 3b 9d	2 ; .
	sla c			;9d09	cb 21		. !
	ld b,000h		;9d0b	06 00		. .
	ld hl,l9d3ah		;9d0d	21 3a 9d	! : .
	sbc hl,bc		;9d10	ed 42		. B
	ld (l9d54h+1),hl	;9d12	22 55 9d	" U .
	exx			;9d15	d9		.
	ld b,c			;9d16	41		A
	push de			;9d17	d5		.
	ld c,0feh		;9d18	0e fe		. .
	inc c			;9d1a	0c		.
	jp l9d54h		;9d1b	c3 54 9d	. T .
	ldi			;9d1e	ed a0		. .
	ldi			;9d20	ed a0		. .
	ldi			;9d22	ed a0		. .
	ldi			;9d24	ed a0		. .
	ldi			;9d26	ed a0		. .
	ldi			;9d28	ed a0		. .
	ldi			;9d2a	ed a0		. .
	ldi			;9d2c	ed a0		. .
	ldi			;9d2e	ed a0		. .
	ldi			;9d30	ed a0		. .
	ldi			;9d32	ed a0		. .
	ldi			;9d34	ed a0		. .
	ldi			;9d36	ed a0		. .
	ldi			;9d38	ed a0		. .
l9d3ah:
	ld de,00000h		;9d3a	11 00 00	. . .
	add hl,de		;9d3d	19		.
	pop de			;9d3e	d1		.
	ld a,d			;9d3f	7a		z
	inc d			;9d40	14		.
	cpl			;9d41	2f		/
	and 007h		;9d42	e6 07		. .
	jp nz,l9d52h		;9d44	c2 52 9d	. R .
	ld a,e			;9d47	7b		{
	add a,020h		;9d48	c6 20		.  
	ld e,a			;9d4a	5f		_
	jp c,l9d52h		;9d4b	da 52 9d	. R .
	ld a,d			;9d4e	7a		z
	sub 008h		;9d4f	d6 08		. .
	ld d,a			;9d51	57		W
l9d52h:
	push de			;9d52	d5		.
	dec b			;9d53	05		.
l9d54h:
	jp nz,l9d54h		;9d54	c2 54 9d	. T .
	pop de			;9d57	d1		.
	ret			;9d58	c9		.
l9d59h:
	nop			;9d59	00		.
sub_9d5ah:
	ld a,(l9b80h)		;9d5a	3a 80 9b	: . .
	and a			;9d5d	a7		.
	ret nz			;9d5e	c0		.
	ld a,(lb7e5h)		;9d5f	3a e5 b7	: . .
	cp 002h			;9d62	fe 02		. .
	jr nz,l9d6ch		;9d64	20 06		  .
	ld a,(lb28fh+1)		;9d66	3a 90 b2	: . .
	cp 078h			;9d69	fe 78		. x
	ret z			;9d6b	c8		.
l9d6ch:
	push iy			;9d6c	fd e5		. .
	exx			;9d6e	d9		.
	ld ix,l9b80h		;9d6f	dd 21 80 9b	. ! . .
	ld (ix+000h),004h	;9d73	dd 36 00 04	. 6 . .
	ld hl,00000h		;9d77	21 00 00	! . .
	ld (la557h),hl		;9d7a	22 57 a5	" W .
	ld hl,(lb28fh+1)	;9d7d	2a 90 b2	* . .
	ld (ix+002h),l		;9d80	dd 75 02	. u .
	ld (ix+004h),h		;9d83	dd 74 04	. t .
	ld a,(l9b68h)		;9d86	3a 68 9b	: h .
	ld (l9d59h),a		;9d89	32 59 9d	2 Y .
	ld a,(lb7e5h)		;9d8c	3a e5 b7	: . .
	cp 002h			;9d8f	fe 02		. .
	jr nz,l9d9eh		;9d91	20 0b		  .
	ld a,l			;9d93	7d		}
	cp 080h			;9d94	fe 80		. .
	jr c,l9d9eh		;9d96	38 06		8 .
	ld a,(l9b52h)		;9d98	3a 52 9b	: R .
	ld (l9d59h),a		;9d9b	32 59 9d	2 Y .
l9d9eh:
	ld (ix+012h),0f0h	;9d9e	dd 36 12 f0	. 6 . .
	ld (ix+013h),060h	;9da2	dd 36 13 60	. 6 . `
	ld (ix+011h),000h	;9da6	dd 36 11 00	. 6 . .
	ld (ix+00ch),010h	;9daa	dd 36 0c 10	. 6 . .
	ld (ix+00dh),008h	;9dae	dd 36 0d 08	. 6 . .
l9db2h:
	call sub_8eb4h		;9db2	cd b4 8e	. . .
	ld a,(l8d49h)		;9db5	3a 49 8d	: I .
	and 00fh		;9db8	e6 0f		. .
	ld hl,l9e4ah		;9dba	21 4a 9e	! J .
	call sub_b5bbh		;9dbd	cd bb b5	. . .
	ld a,(l9d59h)		;9dc0	3a 59 9d	: Y .
	cp (hl)			;9dc3	be		.
	jr z,l9db2h		;9dc4	28 ec		( .
	ld a,(hl)		;9dc6	7e		~
	cp 004h			;9dc7	fe 04		. .
	jr nz,l9df4h		;9dc9	20 29		  )
	ld a,(l9ad0h)		;9dcb	3a d0 9a	: . .
	and a			;9dce	a7		.
	jr z,l9dd8h		;9dcf	28 07		( .
	ld a,(l9ad6h+1)		;9dd1	3a d7 9a	: . .
	cp 002h			;9dd4	fe 02		. .
	jr z,l9db2h		;9dd6	28 da		( .
l9dd8h:
	ld a,(l9ae6h)		;9dd8	3a e6 9a	: . .
	and a			;9ddb	a7		.
	jr z,l9de5h		;9ddc	28 07		( .
	ld a,(09aedh)		;9dde	3a ed 9a	: . .
	cp 002h			;9de1	fe 02		. .
	jr z,l9db2h		;9de3	28 cd		( .
l9de5h:
	ld a,(l9afch)		;9de5	3a fc 9a	: . .
	and a			;9de8	a7		.
	jr z,l9df2h		;9de9	28 07		( .
	ld a,(09b03h)		;9deb	3a 03 9b	: . .
	cp 002h			;9dee	fe 02		. .
	jr z,l9db2h		;9df0	28 c0		( .
l9df2h:
	jr l9e0ah		;9df2	18 16		. .
l9df4h:
	cp 002h			;9df4	fe 02		. .
	jr nz,l9e00h		;9df6	20 08		  .
	ld a,(05cd9h)		;9df8	3a d9 5c	: . \
	dec a			;9dfb	3d		=
	jr nz,l9db2h		;9dfc	20 b4		  .
	jr l9e0ah		;9dfe	18 0a		. .
l9e00h:
	cp 005h			;9e00	fe 05		. .
	jr nz,l9e0ah		;9e02	20 06		  .
	ld a,(la899h)		;9e04	3a 99 a8	: . .
	and a			;9e07	a7		.
	jr nz,l9db2h		;9e08	20 a8		  .
l9e0ah:
	ld a,(hl)		;9e0a	7e		~
	cp 006h			;9e0b	fe 06		. .
	jr nz,l9e23h		;9e0d	20 14		  .
	ld a,(l9bach)		;9e0f	3a ac 9b	: . .
	and a			;9e12	a7		.
	jr nz,l9db2h		;9e13	20 9d		  .
	ld a,(lb7ebh)		;9e15	3a eb b7	: . .
	cp 006h			;9e18	fe 06		. .
	jr c,l9e23h		;9e1a	38 07		8 .
	ld a,(l8d48h)		;9e1c	3a 48 8d	: H .
	and 0c0h		;9e1f	e6 c0		. .
	jr nz,l9db2h		;9e21	20 8f		  .
l9e23h:
	ld a,(hl)		;9e23	7e		~
	dec a			;9e24	3d		=
	jr nz,l9e3ch		;9e25	20 15		  .
	ld a,(lb7e5h)		;9e27	3a e5 b7	: . .
	cp 002h			;9e2a	fe 02		. .
	jr nz,l9e3ch		;9e2c	20 0e		  .
	ld a,(l9b68h)		;9e2e	3a 68 9b	: h .
	dec a			;9e31	3d		=
	jp z,l9db2h		;9e32	ca b2 9d	. . .
	ld a,(l9b52h)		;9e35	3a 52 9b	: R .
	dec a			;9e38	3d		=
	jp z,l9db2h		;9e39	ca b2 9d	. . .
l9e3ch:
	ld a,(hl)		;9e3c	7e		~
	ld (ix+014h),a		;9e3d	dd 77 14	. w .
	ld (ix+001h),a		;9e40	dd 77 01	. w .
	call sub_ab06h		;9e43	cd 06 ab	. . .
	exx			;9e46	d9		.
	pop iy			;9e47	fd e1		. .
	ret			;9e49	c9		.
l9e4ah:
	nop			;9e4a	00		.
	ld bc,00302h		;9e4b	01 02 03	. . .
	inc b			;9e4e	04		.
	dec b			;9e4f	05		.
	ld b,007h		;9e50	06 07		. .
	ex af,af'		;9e52	08		.
	add hl,bc		;9e53	09		.
	nop			;9e54	00		.
	inc b			;9e55	04		.
	nop			;9e56	00		.
	inc bc			;9e57	03		.
	ld bc,00002h		;9e58	01 02 00	. . .
	ld bc,00302h		;9e5b	01 02 03	. . .
	inc b			;9e5e	04		.
	dec b			;9e5f	05		.
	ld b,002h		;9e60	06 02		. .
	ld bc,00003h		;9e62	01 03 00	. . .
	inc b			;9e65	04		.
	nop			;9e66	00		.
	inc bc			;9e67	03		.
	ld bc,00002h		;9e68	01 02 00	. . .
	ld bc,00302h		;9e6b	01 02 03	. . .
	inc b			;9e6e	04		.
	dec b			;9e6f	05		.
	ld b,007h		;9e70	06 07		. .
	ex af,af'		;9e72	08		.
	add hl,bc		;9e73	09		.
	nop			;9e74	00		.
	inc b			;9e75	04		.
	nop			;9e76	00		.
	inc bc			;9e77	03		.
	ld bc,00002h		;9e78	01 02 00	. . .
	ld bc,00302h		;9e7b	01 02 03	. . .
	inc b			;9e7e	04		.
	dec b			;9e7f	05		.
	ld b,002h		;9e80	06 02		. .
	ld bc,00003h		;9e82	01 03 00	. . .
	inc b			;9e85	04		.
	nop			;9e86	00		.
	inc bc			;9e87	03		.
	ld bc,00002h		;9e88	01 02 00	. . .
	ld bc,00302h		;9e8b	01 02 03	. . .
	ld (bc),a		;9e8e	02		.
	nop			;9e8f	00		.
	ld b,007h		;9e90	06 07		. .
	ex af,af'		;9e92	08		.
	add hl,bc		;9e93	09		.
	nop			;9e94	00		.
	inc bc			;9e95	03		.
	nop			;9e96	00		.
	ld (bc),a		;9e97	02		.
	ld bc,00003h		;9e98	01 03 00	. . .
	ld bc,00302h		;9e9b	01 02 03	. . .
	ld (bc),a		;9e9e	02		.
	nop			;9e9f	00		.
	ld b,002h		;9ea0	06 02		. .
	ld bc,00003h		;9ea2	01 03 00	. . .
	inc bc			;9ea5	03		.
	nop			;9ea6	00		.
	ld (bc),a		;9ea7	02		.
	ld bc,03a03h		;9ea8	01 03 3a	. . :
	jp pe,0feb7h		;9eab	ea b7 fe	. . .
	inc b			;9eae	04		.
	ret z			;9eaf	c8		.
	ld a,(l9b68h)		;9eb0	3a 68 9b	: h .
	cp 009h			;9eb3	fe 09		. .
	ret z			;9eb5	c8		.
	ld a,(l9b52h)		;9eb6	3a 52 9b	: R .
	cp 009h			;9eb9	fe 09		. .
	ret z			;9ebb	c8		.
	ld a,(lb7e9h)		;9ebc	3a e9 b7	: . .
	cp 02ch			;9ebf	fe 2c		. ,
	ret nc			;9ec1	d0		.
	ld a,(l9b96h)		;9ec2	3a 96 9b	: . .
	and a			;9ec5	a7		.
	ret nz			;9ec6	c0		.
	ld hl,l9b96h		;9ec7	21 96 9b	! . .
	call 09f37h		;9eca	cd 37 9f	. 7 .
	ld ix,l9b96h		;9ecd	dd 21 96 9b	. ! . .
	ld hl,l9f2bh		;9ed1	21 2b 9f	! + .
	ld a,(lb7ebh)		;9ed4	3a eb b7	: . .
	rra			;9ed7	1f		.
	jr nc,l9eddh		;9ed8	30 03		0 .
	ld hl,09f31h		;9eda	21 31 9f	! 1 .
l9eddh:
	ld a,(hl)		;9edd	7e		~
	ld (ix+000h),a		;9ede	dd 77 00	. w .
	ld (ix+011h),000h	;9ee1	dd 36 11 00	. 6 . .
	inc hl			;9ee5	23		#
	ld a,(hl)		;9ee6	7e		~
	ld (ix+012h),a		;9ee7	dd 77 12	. w .
	inc hl			;9eea	23		#
	ld a,(hl)		;9eeb	7e		~
	ld (ix+013h),a		;9eec	dd 77 13	. w .
	inc hl			;9eef	23		#
	ld a,(hl)		;9ef0	7e		~
	ld (ix+00ch),a		;9ef1	dd 77 0c	. w .
	inc hl			;9ef4	23		#
	ld a,(hl)		;9ef5	7e		~
	ld (ix+00dh),a		;9ef6	dd 77 0d	. w .
	inc hl			;9ef9	23		#
	ld a,(hl)		;9efa	7e		~
	ld (ix+007h),a		;9efb	dd 77 07	. w .
	ld (ix+001h),000h	;9efe	dd 36 01 00	. 6 . .
	ld (ix+004h),000h	;9f02	dd 36 04 00	. 6 . .
	ld a,(l8d48h)		;9f06	3a 48 8d	: H .
	ld hl,l9f27h		;9f09	21 27 9f	! ' .
	and 003h		;9f0c	e6 03		. .
	call sub_b5bbh		;9f0e	cd bb b5	. . .
	ld a,(hl)		;9f11	7e		~
	ld (ix+002h),a		;9f12	dd 77 02	. w .
	ld (ix+006h),010h	;9f15	dd 36 06 10	. 6 . .
	ld (ix+014h),010h	;9f19	dd 36 14 10	. 6 . .
	ld hl,00000h		;9f1d	21 00 00	! . .
l9f20h:
	ld (laa7bh),hl		;9f20	22 7b aa	" { .
	call sub_ab06h		;9f23	cd 06 ab	. . .
	ret			;9f26	c9		.
l9f27h:
	ld b,b			;9f27	40		@
	xor b			;9f28	a8		.
	ld b,b			;9f29	40		@
	xor b			;9f2a	a8		.
l9f2bh:
	add hl,bc		;9f2b	09		.
	ret p			;9f2c	f0		.
	ld (hl),b		;9f2d	70		p
	jr $+14			;9f2e	18 0c		. .
	ld bc,06008h		;9f30	01 08 60	. . `
	sub b			;9f33	90		.
	jr l9f46h		;9f34	18 10		. .
	ld bc,01601h		;9f36	01 01 16	. . .
	nop			;9f39	00		.
	ld b,c			;9f3a	41		A
	jp l8ed9h+1		;9f3b	c3 da 8e	. . .
	ld h,e			;9f3e	63		c
	sbc a,a			;9f3f	9f		.
	ld a,(hl)		;9f40	7e		~
	and d			;9f41	a2		.
	ld a,(hl)		;9f42	7e		~
	and d			;9f43	a2		.
	ld e,d			;9f44	5a		Z
	and l			;9f45	a5		.
l9f46h:
	and e			;9f46	a3		.
	and l			;9f47	a5		.
	sbc a,d			;9f48	9a		.
	xor b			;9f49	a8		.
	jp nc,002a8h		;9f4a	d2 a8 02	. . .
	xor c			;9f4d	a9		.
	cp h			;9f4e	bc		.
	xor c			;9f4f	a9		.
	jr nc,$-84		;9f50	30 aa		0 .
	adc a,l			;9f52	8d		.
	and l			;9f53	a5		.
l9f54h:
	ld hl,09f3ch		;9f54	21 3c 9f	! < .
	ld a,(ix+000h)		;9f57	dd 7e 00	. ~ .
	add a,a			;9f5a	87		.
	call sub_b5bbh		;9f5b	cd bb b5	. . .
	ld a,(hl)		;9f5e	7e		~
	inc hl			;9f5f	23		#
	ld h,(hl)		;9f60	66		f
	ld l,a			;9f61	6f		o
	jp (hl)			;9f62	e9		.
	ret			;9f63	c9		.
sub_9f64h:
	ld a,(l9b70h)		;9f64	3a 70 9b	: p .
	cp 01ch			;9f67	fe 1c		. .
	ld a,000h		;9f69	3e 00		> .
	jr z,l9f6fh		;9f6b	28 02		( .
	ld a,005h		;9f6d	3e 05		> .
l9f6fh:
	ld (05cd8h),a		;9f6f	32 d8 5c	2 . \
	ld a,(ix+002h)		;9f72	dd 7e 02	. ~ .
	and 080h		;9f75	e6 80		. .
	ld (l9653h),a		;9f77	32 53 96	2 S .
	ld bc,(l8ed9h)		;9f7a	ed 4b d9 8e	. K . .
	bit 1,c			;9f7e	cb 49		. I
	ld a,(ix+002h)		;9f80	dd 7e 02	. ~ .
	jr z,l9f87h		;9f83	28 02		( .
	sub 004h		;9f85	d6 04		. .
l9f87h:
	bit 0,c			;9f87	cb 41		. A
	jr z,l9f8dh		;9f89	28 02		( .
	add a,004h		;9f8b	c6 04		. .
l9f8dh:
	ld (ix+002h),a		;9f8d	dd 77 02	. w .
	call sub_a4cfh		;9f90	cd cf a4	. . .
	ld a,(ix+015h)		;9f93	dd 7e 15	. ~ .
	cp 041h			;9f96	fe 41		. A
	jp z,la063h		;9f98	ca 63 a0	. c .
	cp 061h			;9f9b	fe 61		. a
	jp z,la063h		;9f9d	ca 63 a0	. c .
	and 0c1h		;9fa0	e6 c1		. .
	cp 080h			;9fa2	fe 80		. .
	jp z,la063h		;9fa4	ca 63 a0	. c .
	cp 081h			;9fa7	fe 81		. .
	jp z,la063h		;9fa9	ca 63 a0	. c .
	ld b,a			;9fac	47		G
	and 040h		;9fad	e6 40		. @
	jr nz,l9fb8h		;9faf	20 07		  .
	ld a,(la85fh)		;9fb1	3a 5f a8	: _ .
	rla			;9fb4	17		.
	jp c,la063h		;9fb5	da 63 a0	. c .
l9fb8h:
	ld (ix+001h),002h	;9fb8	dd 36 01 02	. 6 . .
	ld (ix+008h),003h	;9fbc	dd 36 08 03	. 6 . .
	ld a,001h		;9fc0	3e 01		> .
	ld (l9b6ah),a		;9fc2	32 6a 9b	2 j .
	ld a,(l8d46h)		;9fc5	3a 46 8d	: F .
	ld e,a			;9fc8	5f		_
	ld a,b			;9fc9	78		x
	and 040h		;9fca	e6 40		. @
	jr nz,la006h		;9fcc	20 38		  8
	bit 0,e			;9fce	cb 43		. C
	jr z,l9fd5h		;9fd0	28 03		( .
	dec (ix+002h)		;9fd2	dd 35 02	. 5 .
l9fd5h:
	call sub_ac6ch		;9fd5	cd 6c ac	. l .
	ld a,(ix+015h)		;9fd8	dd 7e 15	. ~ .
	ld b,a			;9fdb	47		G
	and 01eh		;9fdc	e6 1e		. .
	add a,(ix+002h)		;9fde	dd 86 02	. . .
	add a,008h		;9fe1	c6 08		. .
	ld (l9b6ch),a		;9fe3	32 6c 9b	2 l .
	rr e			;9fe6	cb 1b		. .
	ret c			;9fe8	d8		.
	inc (ix+00ch)		;9fe9	dd 34 0c	. 4 .
	inc (ix+00ch)		;9fec	dd 34 0c	. 4 .
	ld a,b			;9fef	78		x
	add a,002h		;9ff0	c6 02		. .
	or 020h			;9ff2	f6 20		.  
	cp 030h			;9ff4	fe 30		. 0
	jr z,l9ffch		;9ff6	28 04		( .
	ld (ix+015h),a		;9ff8	dd 77 15	. w .
	ret			;9ffb	c9		.
l9ffch:
	ld (ix+015h),081h	;9ffc	dd 36 15 81	. 6 . .
	ld (ix+00ch),02ch	;a000	dd 36 0c 2c	. 6 . ,
	jr la03ah		;a004	18 34		. 4
la006h:
	bit 0,e			;a006	cb 43		. C
	jr z,la00dh		;a008	28 03		( .
	inc (ix+002h)		;a00a	dd 34 02	. 4 .
la00dh:
	call sub_ac6ch		;a00d	cd 6c ac	. l .
	ld a,(ix+015h)		;a010	dd 7e 15	. ~ .
	and 03eh		;a013	e6 3e		. >
	ld b,a			;a015	47		G
	add a,(ix+002h)		;a016	dd 86 02	. . .
	add a,008h		;a019	c6 08		. .
	ld (l9b6ch),a		;a01b	32 6c 9b	2 l .
	rr e			;a01e	cb 1b		. .
	ret c			;a020	d8		.
	dec (ix+00ch)		;a021	dd 35 0c	. 5 .
	dec (ix+00ch)		;a024	dd 35 0c	. 5 .
	ld a,b			;a027	78		x
	sub 002h		;a028	d6 02		. .
	jr c,la032h		;a02a	38 06		8 .
	or 040h			;a02c	f6 40		. @
	ld (ix+015h),a		;a02e	dd 77 15	. w .
	ret			;a031	c9		.
la032h:
	ld (ix+00ch),01ch	;a032	dd 36 0c 1c	. 6 . .
	ld (ix+015h),080h	;a036	dd 36 15 80	. 6 . .
la03ah:
	ld a,(l9b6ah)		;a03a	3a 6a 9b	: j .
	or 080h			;a03d	f6 80		. .
	ld (l9b6ah),a		;a03f	32 6a 9b	2 j .
	ld a,(ix+002h)		;a042	dd 7e 02	. ~ .
	inc a			;a045	3c		<
	and 0fch		;a046	e6 fc		. .
	ld (ix+002h),a		;a048	dd 77 02	. w .
	rra			;a04b	1f		.
	rra			;a04c	1f		.
	and 001h		;a04d	e6 01		. .
	ld b,a			;a04f	47		G
	ld a,(ix+015h)		;a050	dd 7e 15	. ~ .
	and 001h		;a053	e6 01		. .
	add a,a			;a055	87		.
	ld c,a			;a056	4f		O
	add a,a			;a057	87		.
	add a,b			;a058	80		.
	ld (ix+001h),a		;a059	dd 77 01	. w .
	ld a,004h		;a05c	3e 04		> .
	add a,c			;a05e	81		.
	ld (ix+008h),a		;a05f	dd 77 08	. w .
	ret			;a062	c9		.
la063h:
	call sub_aca2h		;a063	cd a2 ac	. . .
	call sub_acbch		;a066	cd bc ac	. . .
	ld a,(la85fh)		;a069	3a 5f a8	: _ .
	and a			;a06c	a7		.
	jp z,la0e1h		;a06d	ca e1 a0	. . .
	ex af,af'		;a070	08		.
	call sub_acbch		;a071	cd bc ac	. . .
	ex af,af'		;a074	08		.
	bit 6,a			;a075	cb 77		. w
	jr z,la0b2h		;a077	28 39		( 9
	ld a,(l8d46h)		;a079	3a 46 8d	: F .
	rra			;a07c	1f		.
	call c,sub_aad2h	;a07d	dc d2 aa	. . .
	ld iy,(lb793h)		;a080	fd 2a 93 b7	. * . .
	ld de,001c8h		;a084	11 c8 01	. . .
	call sub_c25ch		;a087	cd 5c c2	. \ .
	di			;a08a	f3		.
	ld a,(la85fh)		;a08b	3a 5f a8	: _ .
	rla			;a08e	17		.
	ld a,(ix+001h)		;a08f	dd 7e 01	. ~ .
	jr nc,la098h		;a092	30 04		0 .
	and a			;a094	a7		.
	ret nz			;a095	c0		.
	jr la09bh		;a096	18 03		. .
la098h:
	sub 00ah		;a098	d6 0a		. .
	ret nz			;a09a	c0		.
la09bh:
	bit 5,(ix+015h)		;a09b	dd cb 15 6e	. . . n
	jr z,la0a9h		;a09f	28 08		( .
	ld (la85fh),a		;a0a1	32 5f a8	2 _ .
	ld (ix+015h),022h	;a0a4	dd 36 15 22	. 6 . "
	ret			;a0a8	c9		.
la0a9h:
	ld (ix+015h),080h	;a0a9	dd 36 15 80	. 6 . .
	ld (la85fh),a		;a0ad	32 5f a8	2 _ .
	jr la0e1h		;a0b0	18 2f		. /
la0b2h:
	res 7,(ix+015h)		;a0b2	dd cb 15 be	. . . .
	set 0,(ix+015h)		;a0b6	dd cb 15 c6	. . . .
	res 1,(ix+015h)		;a0ba	dd cb 15 8e	. . . .
	set 6,(ix+015h)		;a0be	dd cb 15 f6	. . . .
	rla			;a0c2	17		.
	jr nc,la0d3h		;a0c3	30 0e		0 .
	ld (ix+001h),00ch	;a0c5	dd 36 01 0c	. 6 . .
	ld (ix+013h),0f0h	;a0c9	dd 36 13 f0	. 6 . .
	ld a,0c0h		;a0cd	3e c0		> .
	ld (la85fh),a		;a0cf	32 5f a8	2 _ .
	ret			;a0d2	c9		.
la0d3h:
	ld (ix+013h),0aah	;a0d3	dd 36 13 aa	. 6 . .
	ld (ix+001h),006h	;a0d7	dd 36 01 06	. 6 . .
	ld a,041h		;a0db	3e 41		> A
	ld (la85fh),a		;a0dd	32 5f a8	2 _ .
	ret			;a0e0	c9		.
la0e1h:
	ld a,(ix+002h)		;a0e1	dd 7e 02	. ~ .
	rra			;a0e4	1f		.
	rra			;a0e5	1f		.
	and 001h		;a0e6	e6 01		. .
	ld b,a			;a0e8	47		G
	ld a,(ix+015h)		;a0e9	dd 7e 15	. ~ .
	and 001h		;a0ec	e6 01		. .
	add a,a			;a0ee	87		.
	add a,a			;a0ef	87		.
	add a,b			;a0f0	80		.
	ld (ix+001h),a		;a0f1	dd 77 01	. w .
	ld a,(ix+014h)		;a0f4	dd 7e 14	. ~ .
	dec a			;a0f7	3d		=
	ret nz			;a0f8	c0		.
	ld a,(ix+001h)		;a0f9	dd 7e 01	. ~ .
	add a,00ah		;a0fc	c6 0a		. .
	ld (ix+001h),a		;a0fe	dd 77 01	. w .
	ld a,(la160h)		;a101	3a 60 a1	: ` .
	sub 002h		;a104	d6 02		. .
	jr c,la10ch		;a106	38 04		8 .
	ld (la160h),a		;a108	32 60 a1	2 ` .
	ret			;a10b	c9		.
la10ch:
	ld a,(l8ed9h)		;a10c	3a d9 8e	: . .
	and 010h		;a10f	e6 10		. .
	ret z			;a111	c8		.
	ld iy,l9b12h		;a112	fd 21 12 9b	. ! . .
	ld a,(l9b12h)		;a116	3a 12 9b	: . .
	and a			;a119	a7		.
	jr z,la125h		;a11a	28 09		( .
	ld iy,l9b28h		;a11c	fd 21 28 9b	. ! ( .
	ld a,(l9b28h)		;a120	3a 28 9b	: ( .
	and a			;a123	a7		.
	ret nz			;a124	c0		.
la125h:
	ld (iy+000h),005h	;a125	fd 36 00 05	. 6 . .
	ld (iy+001h),000h	;a129	fd 36 01 00	. 6 . .
	ld (iy+009h),008h	;a12d	fd 36 09 08	. 6 . .
	ld a,(ix+002h)		;a131	dd 7e 02	. ~ .
	add a,00ch		;a134	c6 0c		. .
	ld (iy+002h),a		;a136	fd 77 02	. w .
	ld (iy+004h),0ach	;a139	fd 36 04 ac	. 6 . .
	ld (iy+011h),000h	;a13d	fd 36 11 00	. 6 . .
	ld (iy+015h),000h	;a141	fd 36 15 00	. 6 . .
	ld a,(la160h)		;a145	3a 60 a1	: ` .
	cpl			;a148	2f		/
	and 001h		;a149	e6 01		. .
	add a,016h		;a14b	c6 16		. .
	ld (la160h),a		;a14d	32 60 a1	2 ` .
	push ix			;a150	dd e5		. .
	call sub_c064h		;a152	cd 64 c0	. d .
	ld (ix+000h),00bh	;a155	dd 36 00 0b	. 6 . .
	ld (ix+001h),002h	;a159	dd 36 01 02	. 6 . .
	pop ix			;a15d	dd e1		. .
	ret			;a15f	c9		.
la160h:
	nop			;a160	00		.
sub_a161h:
	ld a,(lb7e5h)		;a161	3a e5 b7	: . .
	cp 002h			;a164	fe 02		. .
	jp nz,sub_a1dbh		;a166	c2 db a1	. . .
	ld a,(lb7efh)		;a169	3a ef b7	: . .
	and a			;a16c	a7		.
	jp nz,sub_a1dbh		;a16d	c2 db a1	. . .
	ld a,(lb7f7h)		;a170	3a f7 b7	: . .
	and a			;a173	a7		.
	jp nz,sub_a1dbh		;a174	c2 db a1	. . .
	ld c,a			;a177	4f		O
	ld a,0fdh		;a178	3e fd		> .
	in a,(0feh)		;a17a	db fe		. .
	cpl			;a17c	2f		/
	and 00ah		;a17d	e6 0a		. .
	jr z,la183h		;a17f	28 02		( .
	set 0,c			;a181	cb c1		. .
la183h:
	ld a,0fdh		;a183	3e fd		> .
	in a,(0feh)		;a185	db fe		. .
	cpl			;a187	2f		/
	and 005h		;a188	e6 05		. .
	jr z,la18eh		;a18a	28 02		( .
	set 1,c			;a18c	cb c9		. .
la18eh:
	ld a,0fah		;a18e	3e fa		> .
	in a,(0feh)		;a190	db fe		. .
	cpl			;a192	2f		/
	and 01fh		;a193	e6 1f		. .
	jr z,la199h		;a195	28 02		( .
	set 4,c			;a197	cb e1		. .
la199h:
	ld a,c			;a199	79		y
	ld (l8ed9h),a		;a19a	32 d9 8e	2 . .
	ret			;a19d	c9		.
sub_a19eh:
	and a			;a19e	a7		.
	jr nz,la1deh		;a19f	20 3d		  =
	ld a,(lb7e5h)		;a1a1	3a e5 b7	: . .
	cp 002h			;a1a4	fe 02		. .
	ld a,(lb7f7h)		;a1a6	3a f7 b7	: . .
	jr nz,la1deh		;a1a9	20 33		  3
	ld a,(lb7efh)		;a1ab	3a ef b7	: . .
	and a			;a1ae	a7		.
	ld a,(lb7f7h)		;a1af	3a f7 b7	: . .
	jr nz,la1deh		;a1b2	20 2a		  *
	ld c,a			;a1b4	4f		O
	ld a,0bfh		;a1b5	3e bf		> .
	in a,(0feh)		;a1b7	db fe		. .
	cpl			;a1b9	2f		/
	and 005h		;a1ba	e6 05		. .
	jr z,la1c0h		;a1bc	28 02		( .
	set 0,c			;a1be	cb c1		. .
la1c0h:
	ld a,0bfh		;a1c0	3e bf		> .
	in a,(0feh)		;a1c2	db fe		. .
	cpl			;a1c4	2f		/
	and 00ah		;a1c5	e6 0a		. .
	jr z,la1cbh		;a1c7	28 02		( .
	set 1,c			;a1c9	cb c9		. .
la1cbh:
	ld a,05fh		;a1cb	3e 5f		> _
	in a,(0feh)		;a1cd	db fe		. .
	cpl			;a1cf	2f		/
	and 01fh		;a1d0	e6 1f		. .
	jr z,la1d6h		;a1d2	28 02		( .
	set 4,c			;a1d4	cb e1		. .
la1d6h:
	ld a,c			;a1d6	79		y
	ld (l8ed9h),a		;a1d7	32 d9 8e	2 . .
	ret			;a1da	c9		.
sub_a1dbh:
	ld a,(lb7efh)		;a1db	3a ef b7	: . .
la1deh:
	and a			;a1de	a7		.
	jp z,la238h		;a1df	ca 38 a2	. 8 .
	dec a			;a1e2	3d		=
	jp z,la1ech		;a1e3	ca ec a1	. . .
	dec a			;a1e6	3d		=
	jp z,la210h		;a1e7	ca 10 a2	. . .
	jr la1f4h		;a1ea	18 08		. .
la1ech:
	in a,(01fh)		;a1ec	db 1f		. .
	and 01fh		;a1ee	e6 1f		. .
	ld c,a			;a1f0	4f		O
	jp la26bh		;a1f1	c3 6b a2	. k .
la1f4h:
	ld a,0efh		;a1f4	3e ef		> .
	in a,(0feh)		;a1f6	db fe		. .
	ld b,0ffh		;a1f8	06 ff		. .
	rra			;a1fa	1f		.
	rl b			;a1fb	cb 10		. .
	rra			;a1fd	1f		.
	rl b			;a1fe	cb 10		. .
	rra			;a200	1f		.
	rl b			;a201	cb 10		. .
	rra			;a203	1f		.
	rra			;a204	1f		.
	rl b			;a205	cb 10		. .
	rla			;a207	17		.
	rl b			;a208	cb 10		. .
	ld a,b			;a20a	78		x
	cpl			;a20b	2f		/
	ld c,a			;a20c	4f		O
	jp la26bh		;a20d	c3 6b a2	. k .
la210h:
	ld a,0efh		;a210	3e ef		> .
	in a,(0feh)		;a212	db fe		. .
	or 0e0h			;a214	f6 e0		. .
	ld b,a			;a216	47		G
	and 008h		;a217	e6 08		. .
	ld c,a			;a219	4f		O
	ld a,b			;a21a	78		x
	rrca			;a21b	0f		.
	rrca			;a21c	0f		.
	ld b,a			;a21d	47		G
	and 005h		;a21e	e6 05		. .
	or c			;a220	b1		.
	ld c,a			;a221	4f		O
	ld a,b			;a222	78		x
	rra			;a223	1f		.
	rra			;a224	1f		.
	and 010h		;a225	e6 10		. .
	or c			;a227	b1		.
	ld c,a			;a228	4f		O
	ld a,0f7h		;a229	3e f7		> .
	in a,(0feh)		;a22b	db fe		. .
	rra			;a22d	1f		.
	rra			;a22e	1f		.
	rra			;a22f	1f		.
	and 002h		;a230	e6 02		. .
	or c			;a232	b1		.
	cpl			;a233	2f		/
	ld c,a			;a234	4f		O
	jp la26bh		;a235	c3 6b a2	. k .
la238h:
	ld c,000h		;a238	0e 00		. .
	ld a,0fdh		;a23a	3e fd		> .
	in a,(0feh)		;a23c	db fe		. .
	and 00ah		;a23e	e6 0a		. .
	ld b,a			;a240	47		G
	ld a,0bfh		;a241	3e bf		> .
	in a,(0feh)		;a243	db fe		. .
	and 015h		;a245	e6 15		. .
	or b			;a247	b0		.
	xor 01fh		;a248	ee 1f		. .
	jr z,la24eh		;a24a	28 02		( .
	set 0,c			;a24c	cb c1		. .
la24eh:
	ld a,0fdh		;a24e	3e fd		> .
	in a,(0feh)		;a250	db fe		. .
	and 015h		;a252	e6 15		. .
	ld b,a			;a254	47		G
	ld a,0bfh		;a255	3e bf		> .
	in a,(0feh)		;a257	db fe		. .
	and 00ah		;a259	e6 0a		. .
	or b			;a25b	b0		.
	xor 01fh		;a25c	ee 1f		. .
	jr z,la262h		;a25e	28 02		( .
	set 1,c			;a260	cb c9		. .
la262h:
	ld a,05ah		;a262	3e 5a		> Z
	call sub_97a7h		;a264	cd a7 97	. . .
	jr z,la26bh		;a267	28 02		( .
	set 4,c			;a269	cb e1		. .
la26bh:
	ld a,c			;a26b	79		y
	ld (l8ed9h),a		;a26c	32 d9 8e	2 . .
	ret			;a26f	c9		.
la270h:
	nop			;a270	00		.
	nop			;a271	00		.
	nop			;a272	00		.
	nop			;a273	00		.
la274h:
	nop			;a274	00		.
	nop			;a275	00		.
	nop			;a276	00		.
	nop			;a277	00		.
la278h:
	nop			;a278	00		.
	nop			;a279	00		.
	nop			;a27a	00		.
	nop			;a27b	00		.
la27ch:
	nop			;a27c	00		.
	nop			;a27d	00		.
la27eh:
	ld a,(ix+012h)		;a27e	dd 7e 12	. ~ .
	and 080h		;a281	e6 80		. .
	ld (l9653h),a		;a283	32 53 96	2 S .
	ld a,(05cdch)		;a286	3a dc 5c	: . \
	inc a			;a289	3c		<
	ld (05cdch),a		;a28a	32 dc 5c	2 . \
	ld (0a7a8h),ix		;a28d	dd 22 a8 a7	. " . .
	push ix			;a291	dd e5		. .
	pop de			;a293	d1		.
	ld bc,la270h		;a294	01 70 a2	. p .
	ld hl,l9ad0h		;a297	21 d0 9a	! . .
	and a			;a29a	a7		.
	sbc hl,de		;a29b	ed 52		. R
	jr z,la2adh		;a29d	28 0e		( .
	ld bc,la274h		;a29f	01 74 a2	. t .
	ld hl,l9ae6h		;a2a2	21 e6 9a	! . .
	and a			;a2a5	a7		.
	sbc hl,de		;a2a6	ed 52		. R
	jr z,la2adh		;a2a8	28 03		( .
	ld bc,la278h		;a2aa	01 78 a2	. x .
la2adh:
	ld (la27ch),bc		;a2ad	ed 43 7c a2	. C | .
	ld l,c			;a2b1	69		i
	ld h,b			;a2b2	60		`
	ld a,(hl)		;a2b3	7e		~
	and a			;a2b4	a7		.
	jr z,la2bbh		;a2b5	28 04		( .
	dec (hl)		;a2b7	35		5
	jp la37fh		;a2b8	c3 7f a3	. . .
la2bbh:
	inc hl			;a2bb	23		#
	ld a,(hl)		;a2bc	7e		~
	and a			;a2bd	a7		.
	jr z,la328h		;a2be	28 68		( h
	add a,(ix+006h)		;a2c0	dd 86 06	. . .
	and 03fh		;a2c3	e6 3f		. ?
	ld (ix+006h),a		;a2c5	dd 77 06	. w .
	inc hl			;a2c8	23		#
	ld b,a			;a2c9	47		G
	add a,002h		;a2ca	c6 02		. .
	and 03ch		;a2cc	e6 3c		. <
	ld (hl),a		;a2ce	77		w
	and 00fh		;a2cf	e6 0f		. .
	jr nz,la2e2h		;a2d1	20 0f		  .
	ld a,b			;a2d3	78		x
	and 00ch		;a2d4	e6 0c		. .
	ld a,(hl)		;a2d6	7e		~
	jr nz,la2ddh		;a2d7	20 04		  .
	add a,004h		;a2d9	c6 04		. .
	jr la2dfh		;a2db	18 02		. .
la2ddh:
	sub 004h		;a2dd	d6 04		. .
la2dfh:
	and 03fh		;a2df	e6 3f		. ?
	ld (hl),a		;a2e1	77		w
la2e2h:
	ld c,(hl)		;a2e2	4e		N
	inc hl			;a2e3	23		#
	ld a,(hl)		;a2e4	7e		~
	exx			;a2e5	d9		.
	ld hl,l8db8h		;a2e6	21 b8 8d	! . .
	call sub_b5bbh		;a2e9	cd bb b5	. . .
	push hl			;a2ec	e5		.
	pop iy			;a2ed	fd e1		. .
	bit 0,(iy+001h)		;a2ef	fd cb 01 46	. . . F
	jr nz,la319h		;a2f3	20 24		  $
	call sub_ac22h		;a2f5	cd 22 ac	. " .
	jr nc,la319h		;a2f8	30 1f		0 .
	exx			;a2fa	d9		.
	push bc			;a2fb	c5		.
	call sub_ad69h		;a2fc	cd 69 ad	. i .
	call sub_ac6ch		;a2ff	cd 6c ac	. l .
	ld e,(ix+006h)		;a302	dd 5e 06	. ^ .
	pop bc			;a305	c1		.
	push de			;a306	d5		.
	ld (ix+006h),c		;a307	dd 71 06	. q .
	push bc			;a30a	c5		.
	call sub_a4a0h		;a30b	cd a0 a4	. . .
	pop bc			;a30e	c1		.
	ld a,(ix+006h)		;a30f	dd 7e 06	. ~ .
	cp c			;a312	b9		.
	pop de			;a313	d1		.
	ret nz			;a314	c0		.
	ld (ix+006h),e		;a315	dd 73 06	. s .
	ret			;a318	c9		.
la319h:
	ld hl,(la27ch)		;a319	2a 7c a2	* | .
	ld (hl),002h		;a31c	36 02		6 .
	inc hl			;a31e	23		#
	ld (hl),000h		;a31f	36 00		6 .
	exx			;a321	d9		.
	ld (ix+006h),c		;a322	dd 71 06	. q .
	jp la490h		;a325	c3 90 a4	. . .
la328h:
	ld a,(l8db7h)		;a328	3a b7 8d	: . .
	and a			;a32b	a7		.
	jp z,la37fh		;a32c	ca 7f a3	. . .
	ld b,a			;a32f	47		G
	ld iy,l8db8h		;a330	fd 21 b8 8d	. ! . .
la334h:
	bit 0,(iy+001h)		;a334	fd cb 01 46	. . . F
	jr nz,la341h		;a338	20 07		  .
	push bc			;a33a	c5		.
	call sub_ac22h		;a33b	cd 22 ac	. " .
	pop bc			;a33e	c1		.
	jr c,la34ah		;a33f	38 09		8 .
la341h:
	ld de,00010h		;a341	11 10 00	. . .
	add iy,de		;a344	fd 19		. .
	djnz la334h		;a346	10 ec		. .
	jr la37fh		;a348	18 35		. 5
la34ah:
	push iy			;a34a	fd e5		. .
	pop hl			;a34c	e1		.
	ld de,l8db8h		;a34d	11 b8 8d	. . .
	and a			;a350	a7		.
	sbc hl,de		;a351	ed 52		. R
	ld e,l			;a353	5d		]
	ld hl,(la27ch)		;a354	2a 7c a2	* | .
	ld (hl),000h		;a357	36 00		6 .
	inc hl			;a359	23		#
	ld b,000h		;a35a	06 00		. .
	ld a,(ix+006h)		;a35c	dd 7e 06	. ~ .
	add a,010h		;a35f	c6 10		. .
	and 03fh		;a361	e6 3f		. ?
	cp 020h			;a363	fe 20		.  
	jr c,la369h		;a365	38 02		8 .
	ld b,0feh		;a367	06 fe		. .
la369h:
	ld c,0ffh		;a369	0e ff		. .
	ld a,(iy+004h)		;a36b	fd 7e 04	. ~ .
	add a,004h		;a36e	c6 04		. .
	cp (ix+004h)		;a370	dd be 04	. . .
	jr c,la379h		;a373	38 04		8 .
	ld a,b			;a375	78		x
	xor 0feh		;a376	ee fe		. .
	ld b,a			;a378	47		G
la379h:
	ld a,c			;a379	79		y
	xor b			;a37a	a8		.
	ld (hl),a		;a37b	77		w
	inc hl			;a37c	23		#
	inc hl			;a37d	23		#
	ld (hl),e		;a37e	73		s
la37fh:
	ld a,(ix+014h)		;a37f	dd 7e 14	. ~ .
	and a			;a382	a7		.
	jp z,la441h		;a383	ca 41 a4	. A .
	dec a			;a386	3d		=
	ld (ix+014h),a		;a387	dd 77 14	. w .
	jr z,la3ceh		;a38a	28 42		( B
	ld a,(lb7e5h)		;a38c	3a e5 b7	: . .
	cp 002h			;a38f	fe 02		. .
	jr nz,la3a7h		;a391	20 14		  .
	ld a,(ix+002h)		;a393	dd 7e 02	. ~ .
	cp 088h			;a396	fe 88		. .
	jr nc,la417h		;a398	30 7d		0 }
	cp 080h			;a39a	fe 80		. .
	jr c,la3a7h		;a39c	38 09		8 .
	ld a,(ix+015h)		;a39e	dd 7e 15	. ~ .
	and 07fh		;a3a1	e6 7f		. .
	cp 00ah			;a3a3	fe 0a		. .
	jr c,la417h		;a3a5	38 70		8 p
la3a7h:
	ld a,(l8ed9h)		;a3a7	3a d9 8e	: . .
	and 010h		;a3aa	e6 10		. .
	jr nz,la3ceh		;a3ac	20 20		   
	ld a,(l9b68h)		;a3ae	3a 68 9b	: h .
	and 07fh		;a3b1	e6 7f		. .
	cp 003h			;a3b3	fe 03		. .
	jr nz,la3ceh		;a3b5	20 17		  .
	ld a,(l9b54h+2)		;a3b7	3a 56 9b	: V .
la3bah:
	ld b,a			;a3ba	47		G
	ld a,(ix+015h)		;a3bb	dd 7e 15	. ~ .
	and 07fh		;a3be	e6 7f		. .
	add a,b			;a3c0	80		.
	ld (ix+002h),a		;a3c1	dd 77 02	. w .
	ld (ix+004h),0a7h	;a3c4	dd 36 04 a7	. 6 . .
	call sub_ac6ch		;a3c8	cd 6c ac	. l .
	jp la4a9h		;a3cb	c3 a9 a4	. . .
la3ceh:
	ld (ix+014h),000h	;a3ce	dd 36 14 00	. 6 . .
	ld (ix+004h),0a9h	;a3d2	dd 36 04 a9	. 6 . .
	ld a,(l9b68h)		;a3d6	3a 68 9b	: h .
	rla			;a3d9	17		.
	jr nc,la3e1h		;a3da	30 05		0 .
	ld a,0ffh		;a3dc	3e ff		> .
	ld (l9b68h),a		;a3de	32 68 9b	2 h .
la3e1h:
	ld a,(ix+015h)		;a3e1	dd 7e 15	. ~ .
	and 07fh		;a3e4	e6 7f		. .
	add a,024h		;a3e6	c6 24		. $
	cp 030h			;a3e8	fe 30		. 0
	jr nz,la3eeh		;a3ea	20 02		  .
	ld a,034h		;a3ec	3e 34		> 4
la3eeh:
	ld (ix+006h),a		;a3ee	dd 77 06	. w .
	ld a,(ix+015h)		;a3f1	dd 7e 15	. ~ .
	and 080h		;a3f4	e6 80		. .
	ld (ix+015h),a		;a3f6	dd 77 15	. w .
	ld (ix+004h),0a6h	;a3f9	dd 36 04 a6	. 6 . .
	ld a,(ix+012h)		;a3fd	dd 7e 12	. ~ .
	and 080h		;a400	e6 80		. .
	ld (ix+012h),a		;a402	dd 77 12	. w .
	push ix			;a405	dd e5		. .
	call sub_c064h		;a407	cd 64 c0	. d .
	ld (ix+000h),004h	;a40a	dd 36 00 04	. 6 . .
	ld (ix+001h),002h	;a40e	dd 36 01 02	. 6 . .
	pop ix			;a412	dd e1		. .
	jp la4a9h		;a414	c3 a9 a4	. . .
la417h:
	ld a,(lb972h)		;a417	3a 72 b9	: r .
	and 010h		;a41a	e6 10		. .
	jr nz,la42ch		;a41c	20 0e		  .
	ld a,(l9b52h)		;a41e	3a 52 9b	: R .
	and 07fh		;a421	e6 7f		. .
	cp 003h			;a423	fe 03		. .
	jr nz,la42ch		;a425	20 05		  .
	ld a,(l9b40h)		;a427	3a 40 9b	: @ .
	jr la3bah		;a42a	18 8e		. .
la42ch:
	ld (ix+014h),000h	;a42c	dd 36 14 00	. 6 . .
	ld (ix+004h),0a9h	;a430	dd 36 04 a9	. 6 . .
	ld a,(l9b52h)		;a434	3a 52 9b	: R .
	rla			;a437	17		.
	jr nc,la3e1h		;a438	30 a7		0 .
	ld a,0ffh		;a43a	3e ff		> .
	ld (l9b52h),a		;a43c	32 52 9b	2 R .
	jr la3e1h		;a43f	18 a0		. .
la441h:
	ld a,(l8d46h)		;a441	3a 46 8d	: F .
	ld c,a			;a444	4f		O
	and 003h		;a445	e6 03		. .
	jr nz,la490h		;a447	20 47		  G
	ld a,(ix+012h)		;a449	dd 7e 12	. ~ .
	inc a			;a44c	3c		<
	ld (ix+012h),a		;a44d	dd 77 12	. w .
	and 07fh		;a450	e6 7f		. .
	cp 07fh			;a452	fe 7f		. .
	jr nz,la473h		;a454	20 1d		  .
	ld a,(ix+012h)		;a456	dd 7e 12	. ~ .
	and 080h		;a459	e6 80		. .
	ld (ix+012h),a		;a45b	dd 77 12	. w .
	ld a,(ix+006h)		;a45e	dd 7e 06	. ~ .
	add a,004h		;a461	c6 04		. .
	and 00fh		;a463	e6 0f		. .
	jr nz,la469h		;a465	20 02		  .
	ld a,004h		;a467	3e 04		> .
la469h:
	ld b,a			;a469	47		G
	ld a,(ix+006h)		;a46a	dd 7e 06	. ~ .
	and 030h		;a46d	e6 30		. 0
	or b			;a46f	b0		.
	ld (ix+006h),a		;a470	dd 77 06	. w .
la473h:
	ld a,c			;a473	79		y
	and 007h		;a474	e6 07		. .
	jr nz,la490h		;a476	20 18		  .
	inc (ix+013h)		;a478	dd 34 13	. 4 .
	ld a,(ix+013h)		;a47b	dd 7e 13	. ~ .
	sub 094h		;a47e	d6 94		. .
	jr nz,la490h		;a480	20 0e		  .
	ld (ix+013h),a		;a482	dd 77 13	. w .
	ld a,(ix+007h)		;a485	dd 7e 07	. ~ .
	cp 006h			;a488	fe 06		. .
	jr z,la490h		;a48a	28 04		( .
	inc a			;a48c	3c		<
	ld (ix+007h),a		;a48d	dd 77 07	. w .
la490h:
	call sub_ad69h		;a490	cd 69 ad	. i .
	ld e,(ix+006h)		;a493	dd 5e 06	. ^ .
	call sub_ac75h		;a496	cd 75 ac	. u .
	ld a,(ix+006h)		;a499	dd 7e 06	. ~ .
	cp e			;a49c	bb		.
	call nz,sub_ab13h	;a49d	c4 13 ab	. . .
sub_a4a0h:
	call sub_ab1fh		;a4a0	cd 1f ab	. . .
	call laffbh+1		;a4a3	cd fc af	. . .
	call sub_a4cfh		;a4a6	cd cf a4	. . .
la4a9h:
	set 7,(ix+015h)		;a4a9	dd cb 15 fe	. . . .
	ld a,(ix+002h)		;a4ad	dd 7e 02	. ~ .
	and 007h		;a4b0	e6 07		. .
	ld (ix+001h),a		;a4b2	dd 77 01	. w .
	ld a,(ix+004h)		;a4b5	dd 7e 04	. ~ .
	cp 0c0h			;a4b8	fe c0		. .
	ret c			;a4ba	d8		.
	ld hl,(la27ch)		;a4bb	2a 7c a2	* | .
	ld (hl),000h		;a4be	36 00		6 .
	inc hl			;a4c0	23		#
	ld (hl),000h		;a4c1	36 00		6 .
	set 7,(ix+000h)		;a4c3	dd cb 00 fe	. . . .
	ld a,(05cd9h)		;a4c7	3a d9 5c	: . \
	dec a			;a4ca	3d		=
	ld (05cd9h),a		;a4cb	32 d9 5c	2 . \
	ret			;a4ce	c9		.
sub_a4cfh:
	ld a,(l9b96h)		;a4cf	3a 96 9b	: . .
	and 07fh		;a4d2	e6 7f		. .
	ret z			;a4d4	c8		.
	cp 00ah			;a4d5	fe 0a		. .
	ret z			;a4d7	c8		.
	ld iy,l9b96h		;a4d8	fd 21 96 9b	. ! . .
	call sub_ac45h		;a4dc	cd 45 ac	. E .
	ret nc			;a4df	d0		.
sub_a4e0h:
	ld (iy+000h),00ah	;a4e0	fd 36 00 0a	. 6 . .
	ld (iy+001h),000h	;a4e4	fd 36 01 00	. 6 . .
	ld (iy+012h),050h	;a4e8	fd 36 12 50	. 6 . P
	ld (iy+013h),090h	;a4ec	fd 36 13 90	. 6 . .
	ld a,(iy+008h)		;a4f0	fd 7e 08	. ~ .
	sub 002h		;a4f3	d6 02		. .
	add a,a			;a4f5	87		.
	add a,a			;a4f6	87		.
	add a,(iy+002h)		;a4f7	fd 86 02	. . .
	ld (iy+002h),a		;a4fa	fd 77 02	. w .
	ld (iy+008h),002h	;a4fd	fd 36 08 02	. 6 . .
	ld (iy+009h),00dh	;a501	fd 36 09 0d	. 6 . .
	ld a,(iy+004h)		;a505	fd 7e 04	. ~ .
	add a,004h		;a508	c6 04		. .
	ld (iy+004h),a		;a50a	fd 77 04	. w .
	ld a,(ix+000h)		;a50d	dd 7e 00	. ~ .
	and 07fh		;a510	e6 7f		. .
	cp 002h			;a512	fe 02		. .
	jr nz,la541h		;a514	20 2b		  +
	ld a,(ix+006h)		;a516	dd 7e 06	. ~ .
	and 010h		;a519	e6 10		. .
	ld de,01030h		;a51b	11 30 10	. 0 .
	jr z,la523h		;a51e	28 03		( .
	ld de,00020h		;a520	11 20 00	.   .
la523h:
	ld a,(l8d49h)		;a523	3a 49 8d	: I .
	ld b,a			;a526	47		G
	rla			;a527	17		.
	jr c,la52bh		;a528	38 01		8 .
	ld e,d			;a52a	5a		Z
la52bh:
	ld a,b			;a52b	78		x
	and 00ch		;a52c	e6 0c		. .
	jr nz,la535h		;a52e	20 05		  .
	ld a,(ix+006h)		;a530	dd 7e 06	. ~ .
	and 00ch		;a533	e6 0c		. .
la535h:
	or e			;a535	b3		.
	ld (ix+006h),a		;a536	dd 77 06	. w .
	ld a,(ix+012h)		;a539	dd 7e 12	. ~ .
	and 080h		;a53c	e6 80		. .
	ld (ix+012h),a		;a53e	dd 77 12	. w .
la541h:
	push ix			;a541	dd e5		. .
	ld ix,lc0cdh		;a543	dd 21 cd c0	. ! . .
	ld (ix+000h),006h	;a547	dd 36 00 06	. 6 . .
	ld (ix+001h),030h	;a54b	dd 36 01 30	. 6 . 0
	pop ix			;a54f	dd e1		. .
	ld bc,00350h		;a551	01 50 03	. P .
	jp l965dh		;a554	c3 5d 96	. ] .
la557h:
	nop			;a557	00		.
la558h:
	nop			;a558	00		.
la559h:
	nop			;a559	00		.
	ld a,(ix+004h)		;a55a	dd 7e 04	. ~ .
	cp 0a0h			;a55d	fe a0		. .
	call nc,sub_a67bh	;a55f	d4 7b a6	. { .
	ld de,00008h		;a562	11 08 00	. . .
	ld b,002h		;a565	06 02		. .
la567h:
	ld hl,(la557h)		;a567	2a 57 a5	* W .
	add hl,de		;a56a	19		.
	ld a,h			;a56b	7c		|
	cp b			;a56c	b8		.
	jr nz,la572h		;a56d	20 03		  .
	ld h,b			;a56f	60		`
	ld l,000h		;a570	2e 00		. .
la572h:
	ld (la557h),hl		;a572	22 57 a5	" W .
	ld d,(ix+004h)		;a575	dd 56 04	. V .
	ld a,(la559h)		;a578	3a 59 a5	: Y .
	ld e,a			;a57b	5f		_
	add hl,de		;a57c	19		.
	ld (ix+004h),h		;a57d	dd 74 04	. t .
	ld a,l			;a580	7d		}
	ld (la559h),a		;a581	32 59 a5	2 Y .
	ld a,h			;a584	7c		|
	cp 0c0h			;a585	fe c0		. .
	ret c			;a587	d8		.
	set 7,(ix+000h)		;a588	dd cb 00 fe	. . . .
	ret			;a58c	c9		.
	ld a,(ix+002h)		;a58d	dd 7e 02	. ~ .
	add a,000h		;a590	c6 00		. .
	ld (ix+002h),a		;a592	dd 77 02	. w .
	call sub_aca2h		;a595	cd a2 ac	. . .
	call sub_acbch		;a598	cd bc ac	. . .
	ld de,00028h		;a59b	11 28 00	. ( .
	ld b,080h		;a59e	06 80		. .
	jp la567h		;a5a0	c3 67 a5	. g .
	ld a,(ix+002h)		;a5a3	dd 7e 02	. ~ .
	and 080h		;a5a6	e6 80		. .
	ld (l9653h),a		;a5a8	32 53 96	2 S .
	ld a,(05cdch)		;a5ab	3a dc 5c	: . \
	inc a			;a5ae	3c		<
	ld (05cdch),a		;a5af	32 dc 5c	2 . \
	ld a,(ix+001h)		;a5b2	dd 7e 01	. ~ .
	cp 002h			;a5b5	fe 02		. .
	jr nc,la5ceh		;a5b7	30 15		0 .
	xor 001h		;a5b9	ee 01		. .
	ld (ix+001h),a		;a5bb	dd 77 01	. w .
	ld a,(ix+004h)		;a5be	dd 7e 04	. ~ .
	sub 006h		;a5c1	d6 06		. .
	ld (ix+004h),a		;a5c3	dd 77 04	. w .
	jr c,la5deh		;a5c6	38 16		8 .
	cp 003h			;a5c8	fe 03		. .
	jr nc,la5ebh		;a5ca	30 1f		0 .
	jr la5deh		;a5cc	18 10		. .
la5ceh:
	ld a,(ix+002h)		;a5ce	dd 7e 02	. ~ .
	and 0f8h		;a5d1	e6 f8		. .
	ld (ix+002h),a		;a5d3	dd 77 02	. w .
	call sub_aad2h		;a5d6	cd d2 aa	. . .
	ld a,(ix+001h)		;a5d9	dd 7e 01	. ~ .
	and a			;a5dc	a7		.
	ret nz			;a5dd	c0		.
la5deh:
	set 7,(ix+000h)		;a5de	dd cb 00 fe	. . . .
	ld a,(la160h)		;a5e2	3a 60 a1	: ` .
	and 001h		;a5e5	e6 01		. .
	ld (la160h),a		;a5e7	32 60 a1	2 ` .
	ret			;a5ea	c9		.
la5ebh:
	call laffbh+1		;a5eb	cd fc af	. . .
	ld a,(l9b96h)		;a5ee	3a 96 9b	: . .
	and 07fh		;a5f1	e6 7f		. .
	ret z			;a5f3	c8		.
	cp 00ah			;a5f4	fe 0a		. .
	ret z			;a5f6	c8		.
	ld iy,l9b96h		;a5f7	fd 21 96 9b	. ! . .
	call sub_ac45h		;a5fb	cd 45 ac	. E .
	ret nc			;a5fe	d0		.
la5ffh:
	ld (iy+000h),00ah	;a5ff	fd 36 00 0a	. 6 . .
	ld (iy+001h),000h	;a603	fd 36 01 00	. 6 . .
	ld (iy+012h),050h	;a607	fd 36 12 50	. 6 . P
	ld (iy+013h),090h	;a60b	fd 36 13 90	. 6 . .
	ld a,(iy+008h)		;a60f	fd 7e 08	. ~ .
	sub 002h		;a612	d6 02		. .
	add a,a			;a614	87		.
	add a,a			;a615	87		.
	add a,(iy+002h)		;a616	fd 86 02	. . .
	ld (iy+002h),a		;a619	fd 77 02	. w .
	ld (iy+008h),002h	;a61c	fd 36 08 02	. 6 . .
	ld (iy+009h),00dh	;a620	fd 36 09 0d	. 6 . .
	ld a,(iy+004h)		;a624	fd 7e 04	. ~ .
	add a,004h		;a627	c6 04		. .
	ld (iy+004h),a		;a629	fd 77 04	. w .
	ld (ix+001h),002h	;a62c	dd 36 01 02	. 6 . .
	ld (ix+009h),006h	;a630	dd 36 09 06	. 6 . .
	ld (ix+012h),050h	;a634	dd 36 12 50	. 6 . P
	ld (ix+013h),050h	;a638	dd 36 13 50	. 6 . P
	ld a,(ix+002h)		;a63c	dd 7e 02	. ~ .
	and 0f8h		;a63f	e6 f8		. .
	ld (ix+002h),a		;a641	dd 77 02	. w .
	push ix			;a644	dd e5		. .
	ld ix,lc0cdh		;a646	dd 21 cd c0	. ! . .
	ld (ix+000h),006h	;a64a	dd 36 00 06	. 6 . .
	ld (ix+001h),030h	;a64e	dd 36 01 30	. 6 . 0
	pop ix			;a652	dd e1		. .
	ld bc,00350h		;a654	01 50 03	. P .
	jp l965dh		;a657	c3 5d 96	. ] .
la65ah:
	nop			;a65a	00		.
sub_a65bh:
	push ix			;a65b	dd e5		. .
	call sub_c064h		;a65d	cd 64 c0	. d .
	ld (ix+000h),00ch	;a660	dd 36 00 0c	. 6 . .
	ld (ix+001h),002h	;a664	dd 36 01 02	. 6 . .
	pop ix			;a668	dd e1		. .
	ret			;a66a	c9		.
la66bh:
	nop			;a66b	00		.
sub_a66ch:
	ld a,(la66bh)		;a66c	3a 6b a6	: k .
	ld b,a			;a66f	47		G
	ld a,(la85fh)		;a670	3a 5f a8	: _ .
	ld (la66bh),a		;a673	32 6b a6	2 k .
	ld a,b			;a676	78		x
	ld (la85fh),a		;a677	32 5f a8	2 _ .
	ret			;a67a	c9		.
sub_a67bh:
	cp 0b0h			;a67b	fe b0		. .
	ret nc			;a67d	d0		.
	ld iy,l9b54h		;a67e	fd 21 54 9b	. ! T .
	call sub_ac45h		;a682	cd 45 ac	. E .
	jr c,la69eh		;a685	38 17		8 .
	ld a,(lb7e5h)		;a687	3a e5 b7	: . .
	cp 002h			;a68a	fe 02		. .
	ret nz			;a68c	c0		.
	ld iy,l9b3eh		;a68d	fd 21 3e 9b	. ! > .
	call sub_ac22h		;a691	cd 22 ac	. " .
	ret nc			;a694	d0		.
	call sub_a66ch		;a695	cd 6c a6	. l .
	call la69eh		;a698	cd 9e a6	. . .
	jp sub_a66ch		;a69b	c3 6c a6	. l .
la69eh:
	ld a,(ix+001h)		;a69e	dd 7e 01	. ~ .
	sub 00ah		;a6a1	d6 0a		. .
	jr nz,la6a9h		;a6a3	20 04		  .
	ld (05cd9h),a		;a6a5	32 d9 5c	2 . \
	ret			;a6a8	c9		.
la6a9h:
	ld a,(iy+002h)		;a6a9	fd 7e 02	. ~ .
	and 080h		;a6ac	e6 80		. .
	ld (l9653h),a		;a6ae	32 53 96	2 S .
	xor a			;a6b1	af		.
	ld (la65ah),a		;a6b2	32 5a a6	2 Z .
	ld bc,00400h		;a6b5	01 00 04	. . .
	call l965dh		;a6b8	cd 5d 96	. ] .
	ld a,(ix+014h)		;a6bb	dd 7e 14	. ~ .
	cp 005h			;a6be	fe 05		. .
	call nz,sub_a65bh	;a6c0	c4 5b a6	. [ .
	dec (iy+014h)		;a6c3	fd 35 14	. 5 .
	jr nz,la6cdh		;a6c6	20 05		  .
	ld a,080h		;a6c8	3e 80		> .
	ld (la85fh),a		;a6ca	32 5f a8	2 _ .
la6cdh:
	ld a,(l8d46h)		;a6cd	3a 46 8d	: F .
	and 001h		;a6d0	e6 01		. .
	inc a			;a6d2	3c		<
	neg			;a6d3	ed 44		. D
	ld (la558h),a		;a6d5	32 58 a5	2 X .
	xor a			;a6d8	af		.
	ld (la557h),a		;a6d9	32 57 a5	2 W .
	ld a,(l8d48h)		;a6dc	3a 48 8d	: H .
	ld b,a			;a6df	47		G
	and 001h		;a6e0	e6 01		. .
	inc a			;a6e2	3c		<
	rl b			;a6e3	cb 10		. .
	jr c,la6e9h		;a6e5	38 02		8 .
	neg			;a6e7	ed 44		. D
la6e9h:
	ld (0a591h),a		;a6e9	32 91 a5	2 . .
	ld (ix+000h),00bh	;a6ec	dd 36 00 0b	. 6 . .
	ld (ix+001h),000h	;a6f0	dd 36 01 00	. 6 . .
	call sub_ab06h		;a6f4	cd 06 ab	. . .
	ld a,(ix+014h)		;a6f7	dd 7e 14	. ~ .
	cp 006h			;a6fa	fe 06		. .
	jp z,laa9dh		;a6fc	ca 9d aa	. . .
	ld (iy+014h),a		;a6ff	fd 77 14	. w .
	ld a,(iy+014h)		;a702	fd 7e 14	. ~ .
	cp 001h			;a705	fe 01		. .
	jr nz,la70eh		;a707	20 05		  .
	ld (la85fh),a		;a709	32 5f a8	2 _ .
	ld a,001h		;a70c	3e 01		> .
la70eh:
	and a			;a70e	a7		.
	jp z,la834h		;a70f	ca 34 a8	. 4 .
	push af			;a712	f5		.
	ld a,(iy+00ch)		;a713	fd 7e 0c	. ~ .
	cp 022h			;a716	fe 22		. "
	jr c,la73dh		;a718	38 23		8 #
	xor a			;a71a	af		.
	ld (l9b7bh),a		;a71b	32 7b 9b	2 { .
	ld (iy+015h),04eh	;a71e	fd 36 15 4e	. 6 . N
	ld (iy+001h),004h	;a722	fd 36 01 04	. 6 . .
	push ix			;a726	dd e5		. .
	call sub_c064h		;a728	cd 64 c0	. d .
	ld (ix+000h),00ah	;a72b	dd 36 00 0a	. 6 . .
	ld (ix+001h),010h	;a72f	dd 36 01 10	. 6 . .
	pop ix			;a733	dd e1		. .
	ld a,(l8d46h)		;a735	3a 46 8d	: F .
	and 0feh		;a738	e6 fe		. .
	ld (l8d46h),a		;a73a	32 46 8d	2 F .
la73dh:
	pop af			;a73d	f1		.
	cp 008h			;a73e	fe 08		. .
	jr nz,la748h		;a740	20 06		  .
	ld bc,05000h		;a742	01 00 50	. . P
	jp l965dh		;a745	c3 5d 96	. ] .
la748h:
	cp 009h			;a748	fe 09		. .
	jr nz,la765h		;a74a	20 19		  .
	ld a,(l9b96h)		;a74c	3a 96 9b	: . .
	and 07fh		;a74f	e6 7f		. .
	ret z			;a751	c8		.
	cp 00ah			;a752	fe 0a		. .
	ret z			;a754	c8		.
	push ix			;a755	dd e5		. .
	ld ix,la748h		;a757	dd 21 48 a7	. ! H .
	ld iy,l9b96h		;a75b	fd 21 96 9b	. ! . .
	call sub_a4e0h		;a75f	cd e0 a4	. . .
	pop ix			;a762	dd e1		. .
	ret			;a764	c9		.
la765h:
	cp 005h			;a765	fe 05		. .
	jp z,la860h		;a767	ca 60 a8	. ` .
	cp 004h			;a76a	fe 04		. .
	jr nz,la79ah		;a76c	20 2c		  ,
	ld (iy+014h),0ffh	;a76e	fd 36 14 ff	. 6 . .
	ld a,002h		;a772	3e 02		> .
	ld (l9ad6h+1),a		;a774	32 d7 9a	2 . .
	ld (09aedh),a		;a777	32 ed 9a	2 . .
	ld (09b03h),a		;a77a	32 03 9b	2 . .
	ld hl,(l8d48h)		;a77d	2a 48 8d	* H .
	ld a,l			;a780	7d		}
	and 01fh		;a781	e6 1f		. .
	add a,01fh		;a783	c6 1f		. .
	ld (l9ae3h),a		;a785	32 e3 9a	2 . .
	ld a,h			;a788	7c		|
	and 01fh		;a789	e6 1f		. .
	add a,01fh		;a78b	c6 1f		. .
	ld (l9af9h),a		;a78d	32 f9 9a	2 . .
	ld a,h			;a790	7c		|
	add a,l			;a791	85		.
	and 01fh		;a792	e6 1f		. .
	add a,01fh		;a794	c6 1f		. .
	ld (l9b0fh),a		;a796	32 0f 9b	2 . .
	ret			;a799	c9		.
la79ah:
	cp 002h			;a79a	fe 02		. .
	ret nz			;a79c	c0		.
	ld a,003h		;a79d	3e 03		> .
	ld (05cd9h),a		;a79f	32 d9 5c	2 . \
	ld (iy+014h),0ffh	;a7a2	fd 36 14 ff	. 6 . .
	ld iy,00000h		;a7a6	fd 21 00 00	. ! . .
	ld l,(iy+002h)		;a7aa	fd 6e 02	. n .
	ld h,(iy+004h)		;a7ad	fd 66 04	. f .
	ld a,(iy+006h)		;a7b0	fd 7e 06	. ~ .
	and 00fh		;a7b3	e6 0f		. .
	ld de,0080ch		;a7b5	11 0c 08	. . .
	cp 004h			;a7b8	fe 04		. .
	jr z,la7c6h		;a7ba	28 0a		( .
	ld de,0040ch		;a7bc	11 0c 04	. . .
	cp 008h			;a7bf	fe 08		. .
	jr z,la7c6h		;a7c1	28 03		( .
	ld de,00408h		;a7c3	11 08 04	. . .
la7c6h:
	ld a,(iy+006h)		;a7c6	fd 7e 06	. ~ .
	and 030h		;a7c9	e6 30		. 0
	or e			;a7cb	b3		.
	ld (0a809h),a		;a7cc	32 09 a8	2 . .
	ld a,(iy+006h)		;a7cf	fd 7e 06	. ~ .
	and 030h		;a7d2	e6 30		. 0
	or d			;a7d4	b2		.
	ld (0a832h),a		;a7d5	32 32 a8	2 2 .
	ld d,(iy+007h)		;a7d8	fd 56 07	. V .
	ld c,(iy+000h)		;a7db	fd 4e 00	. N .
	ld b,(iy+001h)		;a7de	fd 46 01	. F .
	ld iy,l9ad0h		;a7e1	fd 21 d0 9a	. ! . .
	ld a,(l9ad0h)		;a7e5	3a d0 9a	: . .
	and a			;a7e8	a7		.
	jr z,la7efh		;a7e9	28 04		( .
	ld iy,l9ae6h		;a7eb	fd 21 e6 9a	. ! . .
la7efh:
	ld (iy+002h),l		;a7ef	fd 75 02	. u .
	ld (iy+004h),h		;a7f2	fd 74 04	. t .
	ld (iy+000h),002h	;a7f5	fd 36 00 02	. 6 . .
	ld (iy+011h),000h	;a7f9	fd 36 11 00	. 6 . .
	ld (iy+007h),d		;a7fd	fd 72 07	. r .
	ld (iy+000h),c		;a800	fd 71 00	. q .
	ld (iy+001h),b		;a803	fd 70 01	. p .
	ld (iy+006h),000h	;a806	fd 36 06 00	. 6 . .
	ld iy,l9ae6h		;a80a	fd 21 e6 9a	. ! . .
	ld a,(l9ae6h)		;a80e	3a e6 9a	: . .
	and a			;a811	a7		.
	jr z,la818h		;a812	28 04		( .
	ld iy,l9afch		;a814	fd 21 fc 9a	. ! . .
la818h:
	ld (iy+002h),l		;a818	fd 75 02	. u .
	ld (iy+004h),h		;a81b	fd 74 04	. t .
	ld (iy+000h),002h	;a81e	fd 36 00 02	. 6 . .
	ld (iy+011h),000h	;a822	fd 36 11 00	. 6 . .
	ld (iy+007h),d		;a826	fd 72 07	. r .
	ld (iy+000h),c		;a829	fd 71 00	. q .
	ld (iy+001h),b		;a82c	fd 70 01	. p .
	ld (iy+006h),000h	;a82f	fd 36 06 00	. 6 . .
	ret			;a833	c9		.
la834h:
	xor a			;a834	af		.
	ld (l9b7bh),a		;a835	32 7b 9b	2 { .
	ld (iy+015h),020h	;a838	fd 36 15 20	. 6 .  
	ld a,(la85fh)		;a83c	3a 5f a8	: _ .
	and a			;a83f	a7		.
	jr z,la844h		;a840	28 02		( .
	ld a,00ah		;a842	3e 0a		> .
la844h:
	ld (iy+001h),a		;a844	fd 77 01	. w .
	push ix			;a847	dd e5		. .
	call sub_c064h		;a849	cd 64 c0	. d .
	ld (ix+000h),009h	;a84c	dd 36 00 09	. 6 . .
	ld (ix+001h),0c0h	;a850	dd 36 01 c0	. 6 . .
	pop ix			;a854	dd e1		. .
	ld a,(l8d46h)		;a856	3a 46 8d	: F .
	and 0feh		;a859	e6 fe		. .
	ld (l8d46h),a		;a85b	32 46 8d	2 F .
	ret			;a85e	c9		.
la85fh:
	nop			;a85f	00		.
la860h:
	push ix			;a860	dd e5		. .
	ld ix,l9bc2h		;a862	dd 21 c2 9b	. ! . .
	call sub_b684h		;a866	cd 84 b6	. . .
	call sub_9910h		;a869	cd 10 99	. . .
	call sub_9c25h		;a86c	cd 25 9c	. % .
	ld (ix+011h),000h	;a86f	dd 36 11 00	. 6 . .
	ld a,(ix+002h)		;a873	dd 7e 02	. ~ .
	add a,010h		;a876	c6 10		. .
	cp 0e9h			;a878	fe e9		. .
	jr nc,la87fh		;a87a	30 03		0 .
	ld (ix+002h),a		;a87c	dd 77 02	. w .
la87fh:
	call sub_c064h		;a87f	cd 64 c0	. d .
	ld (ix+000h),007h	;a882	dd 36 00 07	. 6 . .
	ld (ix+001h),020h	;a886	dd 36 01 20	. 6 .  
	pop ix			;a88a	dd e1		. .
	ld a,001h		;a88c	3e 01		> .
	ld (la899h),a		;a88e	32 99 a8	2 . .
	ld a,(lb7e8h)		;a891	3a e8 b7	: . .
	inc a			;a894	3c		<
	ld (lb7e8h),a		;a895	32 e8 b7	2 . .
	ret			;a898	c9		.
la899h:
	nop			;a899	00		.
	ld a,(l8d46h)		;a89a	3a 46 8d	: F .
	and 001h		;a89d	e6 01		. .
	ld (ix+001h),a		;a89f	dd 77 01	. w .
	call sub_ab06h		;a8a2	cd 06 ab	. . .
	ld hl,(la8cfh)		;a8a5	2a cf a8	* . .
	ld de,0ffe0h		;a8a8	11 e0 ff	. . .
	add hl,de		;a8ab	19		.
	ld a,(l8d46h)		;a8ac	3a 46 8d	: F .
	cp 038h			;a8af	fe 38		. 8
	jr c,la8b6h		;a8b1	38 03		8 .
	ld (la8cfh),hl		;a8b3	22 cf a8	" . .
la8b6h:
	ld a,(la8d1h)		;a8b6	3a d1 a8	: . .
	ld e,a			;a8b9	5f		_
	ld d,(ix+004h)		;a8ba	dd 56 04	. V .
	add hl,de		;a8bd	19		.
	ld a,l			;a8be	7d		}
	ld (la8d1h),a		;a8bf	32 d1 a8	2 . .
	ld a,h			;a8c2	7c		|
	ld (ix+004h),a		;a8c3	dd 77 04	. w .
	sub 006h		;a8c6	d6 06		. .
	ld (l9b58h),a		;a8c8	32 58 9b	2 X .
	ld (l9b42h),a		;a8cb	32 42 9b	2 B .
	ret			;a8ce	c9		.
la8cfh:
	nop			;a8cf	00		.
	nop			;a8d0	00		.
la8d1h:
	nop			;a8d1	00		.
	call sub_ad69h		;a8d2	cd 69 ad	. i .
	ld a,(ix+004h)		;a8d5	dd 7e 04	. ~ .
	cp 0c0h			;a8d8	fe c0		. .
	jr nc,la8fdh		;a8da	30 21		0 !
	call sub_ac75h		;a8dc	cd 75 ac	. u .
	dec (ix+015h)		;a8df	dd 35 15	. 5 .
	ret nz			;a8e2	c0		.
	ld a,(ix+001h)		;a8e3	dd 7e 01	. ~ .
	cp 004h			;a8e6	fe 04		. .
	jr z,la8fdh		;a8e8	28 13		( .
	inc (ix+001h)		;a8ea	dd 34 01	. 4 .
	call sub_ab06h		;a8ed	cd 06 ab	. . .
	ld a,(ix+014h)		;a8f0	dd 7e 14	. ~ .
	srl a			;a8f3	cb 3f		. ?
	ld (ix+014h),a		;a8f5	dd 77 14	. w .
	inc a			;a8f8	3c		<
	ld (ix+015h),a		;a8f9	dd 77 15	. w .
	ret			;a8fc	c9		.
la8fdh:
	set 7,(ix+000h)		;a8fd	dd cb 00 fe	. . . .
	ret			;a901	c9		.
	ld a,(ix+004h)		;a902	dd 7e 04	. ~ .
	cp 008h			;a905	fe 08		. .
	jr nc,la90dh		;a907	30 04		0 .
	inc (ix+004h)		;a909	dd 34 04	. 4 .
	ret			;a90c	c9		.
la90dh:
	call sub_a977h		;a90d	cd 77 a9	. w .
	ld hl,(laa7bh)		;a910	2a 7b aa	* { .
	ld a,h			;a913	7c		|
	and a			;a914	a7		.
	jr z,la91ch		;a915	28 05		( .
	call sub_aa44h		;a917	cd 44 aa	. D .
	jr la92fh		;a91a	18 13		. .
la91ch:
	ld b,001h		;a91c	06 01		. .
	ld a,(l8d46h)		;a91e	3a 46 8d	: F .
	and 003h		;a921	e6 03		. .
	call z,sub_aa7dh	;a923	cc 7d aa	. } .
	call sub_ad69h		;a926	cd 69 ad	. i .
	call laffbh+1		;a929	cd fc af	. . .
	call sub_ac6ch		;a92c	cd 6c ac	. l .
la92fh:
	ld a,(ix+004h)		;a92f	dd 7e 04	. ~ .
	cp 0c0h			;a932	fe c0		. .
	jr c,la93bh		;a934	38 05		8 .
	set 7,(ix+000h)		;a936	dd cb 00 fe	. . . .
	ret			;a93a	c9		.
la93bh:
	ld a,(l8d46h)		;a93b	3a 46 8d	: F .
	and 000h		;a93e	e6 00		. .
	call z,sub_aad2h	;a940	cc d2 aa	. . .
	ld a,(05cdbh)		;a943	3a db 5c	: . \
	and a			;a946	a7		.
	jp nz,laa94h		;a947	c2 94 aa	. . .
	ret			;a94a	c9		.
	and 004h		;a94b	e6 04		. .
	ld c,a			;a94d	4f		O
	ld a,(ix+006h)		;a94e	dd 7e 06	. ~ .
	add a,010h		;a951	c6 10		. .
	and 03fh		;a953	e6 3f		. ?
	cp 020h			;a955	fe 20		.  
	jr nc,la95bh		;a957	30 02		0 .
	inc c			;a959	0c		.
	inc c			;a95a	0c		.
la95bh:
	ld b,000h		;a95b	06 00		. .
	ld hl,la96fh		;a95d	21 6f a9	! o .
	add hl,bc		;a960	09		.
	ld a,(hl)		;a961	7e		~
	ld (ix+001h),a		;a962	dd 77 01	. w .
	inc hl			;a965	23		#
	ld a,(hl)		;a966	7e		~
	ld (ix+013h),a		;a967	dd 77 13	. w .
	ld (ix+012h),0f0h	;a96a	dd 36 12 f0	. 6 . .
	ret			;a96e	c9		.
la96fh:
	ld bc,00544h		;a96f	01 44 05	. D .
	add a,h			;a972	84		.
	dec c			;a973	0d		.
	ret p			;a974	f0		.
	add hl,bc		;a975	09		.
	ret nz			;a976	c0		.
sub_a977h:
	ld a,(l9b80h)		;a977	3a 80 9b	: . .
	and a			;a97a	a7		.
	ret nz			;a97b	c0		.
	ld a,(l8d48h)		;a97c	3a 48 8d	: H .
	ld b,a			;a97f	47		G
	ld a,(l8d49h)		;a980	3a 49 8d	: I .
	add a,b			;a983	80		.
	and 03fh		;a984	e6 3f		. ?
	ret nz			;a986	c0		.
	ld (l9b91h),a		;a987	32 91 9b	2 . .
	ld a,(ix+004h)		;a98a	dd 7e 04	. ~ .
	add a,008h		;a98d	c6 08		. .
	cp 0c0h			;a98f	fe c0		. .
	ret nc			;a991	d0		.
	ld (l9b84h),a		;a992	32 84 9b	2 . .
	ld a,004h		;a995	3e 04		> .
	ld (l9b80h),a		;a997	32 80 9b	2 . .
	ld a,(ix+002h)		;a99a	dd 7e 02	. ~ .
	add a,008h		;a99d	c6 08		. .
	ld (l9b82h),a		;a99f	32 82 9b	2 . .
	ld a,00ah		;a9a2	3e 0a		> .
	ld (l9b81h),a		;a9a4	32 81 9b	2 . .
	ld a,008h		;a9a7	3e 08		> .
	ld (l9b8ch),a		;a9a9	32 8c 9b	2 . .
	ld (l9b8dh),a		;a9ac	32 8d 9b	2 . .
	ld hl,01002h		;a9af	21 02 10	! . .
	ld (l9b88h),hl		;a9b2	22 88 9b	" . .
	ld hl,00000h		;a9b5	21 00 00	! . .
	ld (la557h),hl		;a9b8	22 57 a5	" W .
	ret			;a9bb	c9		.
	ld a,(ix+004h)		;a9bc	dd 7e 04	. ~ .
	cp 008h			;a9bf	fe 08		. .
	jr nc,la9c7h		;a9c1	30 04		0 .
	inc (ix+004h)		;a9c3	dd 34 04	. 4 .
	ret			;a9c6	c9		.
la9c7h:
	call sub_a977h		;a9c7	cd 77 a9	. w .
	ld a,(ix+006h)		;a9ca	dd 7e 06	. ~ .
	sub 010h		;a9cd	d6 10		. .
	and 03fh		;a9cf	e6 3f		. ?
	ld (0aa03h),a		;a9d1	32 03 aa	2 . .
	ld hl,(laa7bh)		;a9d4	2a 7b aa	* { .
	ld a,h			;a9d7	7c		|
	and a			;a9d8	a7		.
	jr z,la9e0h		;a9d9	28 05		( .
	call sub_aa44h		;a9db	cd 44 aa	. D .
	jr la9f3h		;a9de	18 13		. .
la9e0h:
	ld b,001h		;a9e0	06 01		. .
	ld a,(l8d46h)		;a9e2	3a 46 8d	: F .
	and 003h		;a9e5	e6 03		. .
	call z,sub_aa7dh	;a9e7	cc 7d aa	. } .
	call sub_ad69h		;a9ea	cd 69 ad	. i .
	call laffbh+1		;a9ed	cd fc af	. . .
	call sub_ac6ch		;a9f0	cd 6c ac	. l .
la9f3h:
	ld a,(ix+004h)		;a9f3	dd 7e 04	. ~ .
	cp 0c0h			;a9f6	fe c0		. .
	jr c,la9ffh		;a9f8	38 05		8 .
	set 7,(ix+000h)		;a9fa	dd cb 00 fe	. . . .
	ret			;a9fe	c9		.
la9ffh:
	call sub_aad2h		;a9ff	cd d2 aa	. . .
	ld c,000h		;aa02	0e 00		. .
	ld a,(ix+006h)		;aa04	dd 7e 06	. ~ .
	sub 010h		;aa07	d6 10		. .
	and 03fh		;aa09	e6 3f		. ?
	xor c			;aa0b	a9		.
	and 020h		;aa0c	e6 20		.  
	jr z,laa28h		;aa0e	28 18		( .
	ld a,(ix+013h)		;aa10	dd 7e 13	. ~ .
	ld (ix+013h),a		;aa13	dd 77 13	. w .
	bit 5,c			;aa16	cb 69		. i
	jr z,laa21h		;aa18	28 07		( .
	ld a,00eh		;aa1a	3e 0e		> .
	sub (ix+001h)		;aa1c	dd 96 01	. . .
	jr laa28h		;aa1f	18 07		. .
laa21h:
	ld a,(ix+001h)		;aa21	dd 7e 01	. ~ .
	xor 007h		;aa24	ee 07		. .
	add a,007h		;aa26	c6 07		. .
laa28h:
	ld a,(05cdbh)		;aa28	3a db 5c	: . \
	and a			;aa2b	a7		.
	jp nz,laa94h		;aa2c	c2 94 aa	. . .
	ret			;aa2f	c9		.
	ld (ix+013h),090h	;aa30	dd 36 13 90	. 6 . .
	call sub_aad2h		;aa34	cd d2 aa	. . .
	ld a,(ix+001h)		;aa37	dd 7e 01	. ~ .
	and 03fh		;aa3a	e6 3f		. ?
	cp 009h			;aa3c	fe 09		. .
	ret nz			;aa3e	c0		.
	set 7,(ix+000h)		;aa3f	dd cb 00 fe	. . . .
	ret			;aa43	c9		.
sub_aa44h:
	ld a,l			;aa44	7d		}
	cp 010h			;aa45	fe 10		. .
	jr nc,laa4eh		;aa47	30 05		0 .
	ld l,010h		;aa49	2e 10		. .
	ld (laa7bh),hl		;aa4b	22 7b aa	" { .
laa4eh:
	ld a,(ix+002h)		;aa4e	dd 7e 02	. ~ .
	cp l			;aa51	bd		.
	jr z,laa5fh		;aa52	28 0b		( .
	jr c,laa5ch		;aa54	38 06		8 .
	dec (ix+002h)		;aa56	dd 35 02	. 5 .
	dec (ix+002h)		;aa59	dd 35 02	. 5 .
laa5ch:
	inc (ix+002h)		;aa5c	dd 34 02	. 4 .
laa5fh:
	ld a,(ix+004h)		;aa5f	dd 7e 04	. ~ .
	cp h			;aa62	bc		.
	jr z,laa6fh		;aa63	28 0a		( .
	jr c,laa6bh		;aa65	38 04		8 .
	dec (ix+004h)		;aa67	dd 35 04	. 5 .
	ret			;aa6a	c9		.
laa6bh:
	inc (ix+004h)		;aa6b	dd 34 04	. 4 .
	ret			;aa6e	c9		.
laa6fh:
	ld a,(ix+002h)		;aa6f	dd 7e 02	. ~ .
	cp l			;aa72	bd		.
	ret nz			;aa73	c0		.
	ld hl,00000h		;aa74	21 00 00	! . .
	ld (laa7bh),hl		;aa77	22 7b aa	" { .
	ret			;aa7a	c9		.
laa7bh:
	nop			;aa7b	00		.
	nop			;aa7c	00		.
sub_aa7dh:
	ld a,(ix+006h)		;aa7d	dd 7e 06	. ~ .
	ld l,a			;aa80	6f		o
	sub (ix+014h)		;aa81	dd 96 14	. . .
	jr z,laa94h		;aa84	28 0e		( .
	bit 5,a			;aa86	cb 6f		. o
	ld a,b			;aa88	78		x
	jr nz,laa8dh		;aa89	20 02		  .
	neg			;aa8b	ed 44		. D
laa8dh:
	add a,l			;aa8d	85		.
	and 03fh		;aa8e	e6 3f		. ?
	ld (ix+006h),a		;aa90	dd 77 06	. w .
	ret			;aa93	c9		.
laa94h:
	ld a,(l8d48h)		;aa94	3a 48 8d	: H .
	and 03fh		;aa97	e6 3f		. ?
	ld (ix+014h),a		;aa99	dd 77 14	. w .
	ret			;aa9c	c9		.
laa9dh:
	ld a,006h		;aa9d	3e 06		> .
	ld (l9bach),a		;aa9f	32 ac 9b	2 . .
	xor a			;aaa2	af		.
	ld (l9bbdh),a		;aaa3	32 bd 9b	2 . .
	ld (0bb84h),iy		;aaa6	fd 22 84 bb	. " . .
	ld a,(iy+00ch)		;aaaa	fd 7e 0c	. ~ .
	cp 01ch			;aaad	fe 1c		. .
	ld a,004h		;aaaf	3e 04		> .
	jr z,laab5h		;aab1	28 02		( .
	ld a,00ch		;aab3	3e 0c		> .
laab5h:
	add a,(iy+002h)		;aab5	fd 86 02	. . .
	ld (l9baeh),a		;aab8	32 ae 9b	2 . .
	ld a,(iy+004h)		;aabb	fd 7e 04	. ~ .
	add a,006h		;aabe	c6 06		. .
	ld (l9bb0h),a		;aac0	32 b0 9b	2 . .
	ld a,01bh		;aac3	3e 1b		> .
	ld (l891dh),a		;aac5	32 1d 89	2 . .
	ld hl,00000h		;aac8	21 00 00	! . .
	ld (la8cfh),hl		;aacb	22 cf a8	" . .
	inc (iy+014h)		;aace	fd 34 14	. 4 .
	ret			;aad1	c9		.
sub_aad2h:
	ld a,(ix+012h)		;aad2	dd 7e 12	. ~ .
	ld b,a			;aad5	47		G
	sub 040h		;aad6	d6 40		. @
	jr nc,lab02h		;aad8	30 28		0 (
	ld a,(ix+001h)		;aada	dd 7e 01	. ~ .
	and 03fh		;aadd	e6 3f		. ?
	inc a			;aadf	3c		<
	ld e,a			;aae0	5f		_
	ld a,(ix+013h)		;aae1	dd 7e 13	. ~ .
	ld d,a			;aae4	57		W
	rrca			;aae5	0f		.
	rrca			;aae6	0f		.
	rrca			;aae7	0f		.
	rrca			;aae8	0f		.
	and 00fh		;aae9	e6 0f		. .
	cp e			;aaeb	bb		.
	jr nc,laaf2h		;aaec	30 04		0 .
	ld a,d			;aaee	7a		z
	and 00fh		;aaef	e6 0f		. .
	ld e,a			;aaf1	5f		_
laaf2h:
	ld (ix+001h),e		;aaf2	dd 73 01	. s .
	ld a,b			;aaf5	78		x
	add a,a			;aaf6	87		.
	add a,a			;aaf7	87		.
	and 0c0h		;aaf8	e6 c0		. .
	or (ix+012h)		;aafa	dd b6 12	. . .
	ex af,af'		;aafd	08		.
	call sub_ab06h		;aafe	cd 06 ab	. . .
	ex af,af'		;ab01	08		.
lab02h:
	ld (ix+012h),a		;ab02	dd 77 12	. w .
	ret			;ab05	c9		.
sub_ab06h:
	call sub_7767h		;ab06	cd 67 77	. g w
	ld a,(de)		;ab09	1a		.
	ld (ix+008h),a		;ab0a	dd 77 08	. w .
	inc de			;ab0d	13		.
	ld a,(de)		;ab0e	1a		.
	ld (ix+009h),a		;ab0f	dd 77 09	. w .
	ret			;ab12	c9		.
sub_ab13h:
	push ix			;ab13	dd e5		. .
	call sub_c064h		;ab15	cd 64 c0	. d .
	ld (ix+000h),003h	;ab18	dd 36 00 03	. 6 . .
	pop ix			;ab1c	dd e1		. .
	ret			;ab1e	c9		.
sub_ab1fh:
	ld a,(ix+004h)		;ab1f	dd 7e 04	. ~ .
	cp 098h			;ab22	fe 98		. .
	ret c			;ab24	d8		.
	ld a,(ix+00fh)		;ab25	dd 7e 0f	. ~ .
	cp 0aah			;ab28	fe aa		. .
	ret nc			;ab2a	d0		.
	ld iy,l9b54h		;ab2b	fd 21 54 9b	. ! T .
	call sub_ac22h		;ab2f	cd 22 ac	. " .
	jr c,lab42h		;ab32	38 0e		8 .
	ld a,(lb7e5h)		;ab34	3a e5 b7	: . .
	cp 002h			;ab37	fe 02		. .
	ret nz			;ab39	c0		.
	ld iy,l9b3eh		;ab3a	fd 21 3e 9b	. ! > .
	call sub_ac22h		;ab3e	cd 22 ac	. " .
	ret nc			;ab41	d0		.
lab42h:
	res 7,(ix+012h)		;ab42	dd cb 12 be	. . . .
	bit 7,(iy+002h)		;ab46	fd cb 02 7e	. . . ~
	jr z,lab50h		;ab4a	28 04		( .
	set 7,(ix+012h)		;ab4c	dd cb 12 fe	. . . .
lab50h:
	call sub_ab13h		;ab50	cd 13 ab	. . .
	ld a,(iy+014h)		;ab53	fd 7e 14	. ~ .
	cp 003h			;ab56	fe 03		. .
	jr nz,lab85h		;ab58	20 2b		  +
	ld a,(iy+00ch)		;ab5a	fd 7e 0c	. ~ .
	cp 01ch			;ab5d	fe 1c		. .
	jr nz,lab85h		;ab5f	20 24		  $
	ld a,(ix+002h)		;ab61	dd 7e 02	. ~ .
	sub (iy+002h)		;ab64	fd 96 02	. . .
	jr nc,lab6ah		;ab67	30 01		0 .
	xor a			;ab69	af		.
lab6ah:
	and 0fch		;ab6a	e6 fc		. .
	cp 019h			;ab6c	fe 19		. .
	jr c,lab72h		;ab6e	38 02		8 .
	ld a,018h		;ab70	3e 18		> .
lab72h:
	ld b,a			;ab72	47		G
	ld a,(ix+015h)		;ab73	dd 7e 15	. ~ .
	and 080h		;ab76	e6 80		. .
	or b			;ab78	b0		.
	ld (ix+015h),a		;ab79	dd 77 15	. w .
	ld (ix+014h),0b0h	;ab7c	dd 36 14 b0	. 6 . .
	ld (ix+004h),0a7h	;ab80	dd 36 04 a7	. 6 . .
	ret			;ab84	c9		.
lab85h:
	ld (ix+004h),0a6h	;ab85	dd 36 04 a6	. 6 . .
	ld a,(ix+012h)		;ab89	dd 7e 12	. ~ .
	and 080h		;ab8c	e6 80		. .
	ld (ix+012h),a		;ab8e	dd 77 12	. w .
	ld a,(iy+00ch)		;ab91	fd 7e 0c	. ~ .
	ld hl,labeeh		;ab94	21 ee ab	! . .
	cp 01ch			;ab97	fe 1c		. .
	jr z,lab9eh		;ab99	28 03		( .
	ld hl,labfch		;ab9b	21 fc ab	! . .
lab9eh:
	ld a,(ix+002h)		;ab9e	dd 7e 02	. ~ .
	add a,003h		;aba1	c6 03		. .
	sub (iy+002h)		;aba3	fd 96 02	. . .
	jr c,labafh		;aba6	38 07		8 .
laba8h:
	cp (hl)			;aba8	be		.
	jr c,labafh		;aba9	38 04		8 .
	inc hl			;abab	23		#
	inc hl			;abac	23		#
	jr laba8h		;abad	18 f9		. .
labafh:
	inc hl			;abaf	23		#
	ld a,(hl)		;abb0	7e		~
	bit 2,a			;abb1	cb 57		. W
	jr nz,labb8h		;abb3	20 03		  .
	jp labcbh		;abb5	c3 cb ab	. . .
labb8h:
	call sub_abbfh		;abb8	cd bf ab	. . .
	ld a,(hl)		;abbb	7e		~
	call labcbh		;abbc	cd cb ab	. . .
sub_abbfh:
	ld a,(ix+006h)		;abbf	dd 7e 06	. ~ .
	xor 01fh		;abc2	ee 1f		. .
	inc a			;abc4	3c		<
	and 03fh		;abc5	e6 3f		. ?
	ld (ix+006h),a		;abc7	dd 77 06	. w .
	ret			;abca	c9		.
labcbh:
	and 003h		;abcb	e6 03		. .
	add a,a			;abcd	87		.
	ld b,a			;abce	47		G
	add a,a			;abcf	87		.
	add a,b			;abd0	80		.
	ld hl,lac0ah		;abd1	21 0a ac	! . .
	call sub_b5bbh		;abd4	cd bb b5	. . .
	ld a,004h		;abd7	3e 04		> .
labd9h:
	cp (ix+006h)		;abd9	dd be 06	. . .
	jr z,labe9h		;abdc	28 0b		( .
	inc hl			;abde	23		#
	add a,004h		;abdf	c6 04		. .
	cp 010h			;abe1	fe 10		. .
	jr nz,labd9h		;abe3	20 f4		  .
	add a,004h		;abe5	c6 04		. .
	jr labd9h		;abe7	18 f0		. .
labe9h:
	ld a,(hl)		;abe9	7e		~
	ld (ix+006h),a		;abea	dd 77 06	. w .
	ret			;abed	c9		.
labeeh:
	inc b			;abee	04		.
	rlca			;abef	07		.
	ex af,af'		;abf0	08		.
	ld b,00ch		;abf1	06 0c		. .
	dec b			;abf3	05		.
	djnz labf6h		;abf4	10 00		. .
labf6h:
	inc d			;abf6	14		.
	ld bc,00218h		;abf7	01 18 02	. . .
	rst 38h			;abfa	ff		.
	inc bc			;abfb	03		.
labfch:
	ld b,007h		;abfc	06 07		. .
	inc c			;abfe	0c		.
	ld b,012h		;abff	06 12		. .
	dec b			;ac01	05		.
	ld a,(de)		;ac02	1a		.
	nop			;ac03	00		.
	jr nz,$+3		;ac04	20 01		  .
	ld h,002h		;ac06	26 02		& .
	rst 38h			;ac08	ff		.
	inc bc			;ac09	03		.
lac0ah:
	inc a			;ac0a	3c		<
	jr c,lac41h		;ac0b	38 34		8 4
	inc l			;ac0d	2c		,
	jr z,$+38		;ac0e	28 24		( $
	inc a			;ac10	3c		<
	jr c,$+54		;ac11	38 34		8 4
	inc (hl)		;ac13	34		4
	inc (hl)		;ac14	34		4
	inc (hl)		;ac15	34		4
	inc a			;ac16	3c		<
	jr c,lac51h		;ac17	38 38		8 8
	inc (hl)		;ac19	34		4
	jr c,lac54h		;ac1a	38 38		8 8
	inc a			;ac1c	3c		<
	inc a			;ac1d	3c		<
	jr c,lac58h		;ac1e	38 38		8 8
	inc a			;ac20	3c		<
	inc a			;ac21	3c		<
sub_ac22h:
	ld l,(ix+002h)		;ac22	dd 6e 02	. n .
	ld a,(iy+002h)		;ac25	fd 7e 02	. ~ .
	ld c,(ix+00ch)		;ac28	dd 4e 0c	. N .
	ld b,(iy+00ch)		;ac2b	fd 46 0c	. F .
	call sub_ac3eh		;ac2e	cd 3e ac	. > .
	ret nc			;ac31	d0		.
	ld l,(ix+004h)		;ac32	dd 6e 04	. n .
	ld a,(iy+004h)		;ac35	fd 7e 04	. ~ .
	ld c,(ix+00dh)		;ac38	dd 4e 0d	. N .
	ld b,(iy+00dh)		;ac3b	fd 46 0d	. F .
sub_ac3eh:
	sub l			;ac3e	95		.
	jr c,lac43h		;ac3f	38 02		8 .
lac41h:
	sub c			;ac41	91		.
	ret			;ac42	c9		.
lac43h:
	add a,b			;ac43	80		.
	ret			;ac44	c9		.
sub_ac45h:
	ld l,(ix+002h)		;ac45	dd 6e 02	. n .
	ld a,(iy+002h)		;ac48	fd 7e 02	. ~ .
	ld c,(ix+00ch)		;ac4b	dd 4e 0c	. N .
	ld b,(iy+00ch)		;ac4e	fd 46 0c	. F .
lac51h:
	call sub_ac61h		;ac51	cd 61 ac	. a .
lac54h:
	ret nc			;ac54	d0		.
	ld l,(ix+004h)		;ac55	dd 6e 04	. n .
lac58h:
	ld a,(iy+004h)		;ac58	fd 7e 04	. ~ .
	ld c,(ix+00dh)		;ac5b	dd 4e 0d	. N .
	ld b,(iy+00dh)		;ac5e	fd 46 0d	. F .
sub_ac61h:
	sub l			;ac61	95		.
	jr c,lac68h		;ac62	38 04		8 .
	dec c			;ac64	0d		.
	dec c			;ac65	0d		.
	sub c			;ac66	91		.
	ret			;ac67	c9		.
lac68h:
	dec b			;ac68	05		.
	dec b			;ac69	05		.
	add a,b			;ac6a	80		.
	ret			;ac6b	c9		.
sub_ac6ch:
	call sub_ac97h		;ac6c	cd 97 ac	. . .
	call sub_aca2h		;ac6f	cd a2 ac	. . .
	jp sub_acbch		;ac72	c3 bc ac	. . .
sub_ac75h:
	ld b,03fh		;ac75	06 3f		. ?
	call sub_ac97h		;ac77	cd 97 ac	. . .
	call c,sub_aceeh	;ac7a	dc ee ac	. . .
	ld b,01fh		;ac7d	06 1f		. .
	call sub_aca2h		;ac7f	cd a2 ac	. . .
	call c,sub_aceeh	;ac82	dc ee ac	. . .
	call sub_acbch		;ac85	cd bc ac	. . .
	ret c			;ac88	d8		.
	jp sub_aceeh		;ac89	c3 ee ac	. . .
	ld a,(ix+004h)		;ac8c	dd 7e 04	. ~ .
	cp 0afh			;ac8f	fe af		. .
	ret c			;ac91	d8		.
	ld (ix+004h),0afh	;ac92	dd 36 04 af	. 6 . .
	ret			;ac96	c9		.
sub_ac97h:
	ld a,(ix+004h)		;ac97	dd 7e 04	. ~ .
	cp 008h			;ac9a	fe 08		. .
	ret nc			;ac9c	d0		.
	ld (ix+004h),008h	;ac9d	dd 36 04 08	. 6 . .
	ret			;aca1	c9		.
sub_aca2h:
	ld a,(ix+002h)		;aca2	dd 7e 02	. ~ .
	cp 008h			;aca5	fe 08		. .
	ret nc			;aca7	d0		.
	ld (ix+002h),008h	;aca8	dd 36 02 08	. 6 . .
	ret			;acac	c9		.
sub_acadh:
	ld a,(ix+002h)		;acad	dd 7e 02	. ~ .
	cp 080h			;acb0	fe 80		. .
	ret nc			;acb2	d0		.
	ld (ix+002h),080h	;acb3	dd 36 02 80	. 6 . .
	res 0,(ix+001h)		;acb7	dd cb 01 86	. . . .
	ret			;acbb	c9		.
sub_acbch:
	ld a,(ix+00ch)		;acbc	dd 7e 0c	. ~ .
	add a,(ix+002h)		;acbf	dd 86 02	. . .
	cp 0f9h			;acc2	fe f9		. .
	ret c			;acc4	d8		.
	ld a,0f8h		;acc5	3e f8		> .
	sub (ix+00ch)		;acc7	dd 96 0c	. . .
	ld (ix+002h),a		;acca	dd 77 02	. w .
	ret			;accd	c9		.
sub_acceh:
	ld a,(ix+00ch)		;acce	dd 7e 0c	. ~ .
	add a,(ix+002h)		;acd1	dd 86 02	. . .
	cp 080h			;acd4	fe 80		. .
	ret c			;acd6	d8		.
	ld a,080h		;acd7	3e 80		> .
	sub (ix+00ch)		;acd9	dd 96 0c	. . .
	ld (ix+002h),a		;acdc	dd 77 02	. w .
	ld a,(ix+00ch)		;acdf	dd 7e 0c	. ~ .
	cp 01ch			;ace2	fe 1c		. .
	jr z,lace9h		;ace4	28 03		( .
	cp 02ch			;ace6	fe 2c		. ,
	ret nz			;ace8	c0		.
lace9h:
	set 0,(ix+001h)		;ace9	dd cb 01 c6	. . . .
	ret			;aced	c9		.
sub_aceeh:
	ld a,(ix+006h)		;acee	dd 7e 06	. ~ .
	xor b			;acf1	a8		.
	inc a			;acf2	3c		<
	and 03fh		;acf3	e6 3f		. ?
	ld (ix+006h),a		;acf5	dd 77 06	. w .
	ret			;acf8	c9		.
sub_acf9h:
	ld h,000h		;acf9	26 00		& .
	ld b,h			;acfb	44		D
	ld l,h			;acfc	6c		l
	ld d,(ix+007h)		;acfd	dd 56 07	. V .
	ld a,008h		;ad00	3e 08		> .
	jr lad0ah		;ad02	18 06		. .
lad04h:
	dec a			;ad04	3d		=
	ret z			;ad05	c8		.
	sla c			;ad06	cb 21		. !
	rl b			;ad08	cb 10		. .
lad0ah:
	srl d			;ad0a	cb 3a		. :
	jr nc,lad04h		;ad0c	30 f6		0 .
	add hl,bc		;ad0e	09		.
	jp nz,lad04h		;ad0f	c2 04 ad	. . .
	ret			;ad12	c9		.
sub_ad13h:
	push bc			;ad13	c5		.
	call sub_acf9h		;ad14	cd f9 ac	. . .
	pop bc			;ad17	c1		.
	inc b			;ad18	04		.
	ret nz			;ad19	c0		.
	ld a,l			;ad1a	7d		}
	cpl			;ad1b	2f		/
	ld l,a			;ad1c	6f		o
	ld a,h			;ad1d	7c		|
	cpl			;ad1e	2f		/
	ld h,a			;ad1f	67		g
	inc hl			;ad20	23		#
	ret			;ad21	c9		.
sub_ad22h:
	ld hl,lad58h		;ad22	21 58 ad	! X .
	ld a,(ix+006h)		;ad25	dd 7e 06	. ~ .
	and 00fh		;ad28	e6 0f		. .
	ld b,a			;ad2a	47		G
	call sub_b5bbh		;ad2b	cd bb b5	. . .
	ld c,(hl)		;ad2e	4e		N
	ld a,b			;ad2f	78		x
	xor 00fh		;ad30	ee 0f		. .
	inc a			;ad32	3c		<
	ld hl,lad58h		;ad33	21 58 ad	! X .
	call sub_b5bbh		;ad36	cd bb b5	. . .
	ld l,(hl)		;ad39	6e		n
	ld h,000h		;ad3a	26 00		& .
	ld b,h			;ad3c	44		D
	ld a,(ix+006h)		;ad3d	dd 7e 06	. ~ .
	and 030h		;ad40	e6 30		. 0
	ret z			;ad42	c8		.
	cp 010h			;ad43	fe 10		. .
	jr nz,lad4ch		;ad45	20 05		  .
	ld a,l			;ad47	7d		}
	ld l,c			;ad48	69		i
	ld c,a			;ad49	4f		O
	dec b			;ad4a	05		.
	ret			;ad4b	c9		.
lad4ch:
	cp 020h			;ad4c	fe 20		.  
	jr nz,lad53h		;ad4e	20 03		  .
	dec h			;ad50	25		%
	dec b			;ad51	05		.
	ret			;ad52	c9		.
lad53h:
	ld a,c			;ad53	79		y
	ld c,l			;ad54	4d		M
	ld l,a			;ad55	6f		o
	dec h			;ad56	25		%
	ret			;ad57	c9		.
lad58h:
	rst 38h			;ad58	ff		.
	defb 0fdh,0fah,0f4h ;illegal sequence	;ad59	fd fa f4	. . .
	and 0e0h		;ad5c	e6 e0		. .
	call nc,0b4c5h		;ad5e	d4 c5 b4	. . .
	and c			;ad61	a1		.
	adc a,l			;ad62	8d		.
	ld a,b			;ad63	78		x
	ld h,c			;ad64	61		a
	ld c,d			;ad65	4a		J
	ld sp,00018h		;ad66	31 18 00	1 . .
sub_ad69h:
	call sub_ad22h		;ad69	cd 22 ad	. " .
	push hl			;ad6c	e5		.
	call sub_ad13h		;ad6d	cd 13 ad	. . .
	ld d,(ix+002h)		;ad70	dd 56 02	. V .
	ld e,(ix+003h)		;ad73	dd 5e 03	. ^ .
	add hl,de		;ad76	19		.
	ld (ix+002h),h		;ad77	dd 74 02	. t .
	ld (ix+003h),l		;ad7a	dd 75 03	. u .
	pop bc			;ad7d	c1		.
	call sub_ad13h		;ad7e	cd 13 ad	. . .
	ld d,(ix+004h)		;ad81	dd 56 04	. V .
	ld e,(ix+005h)		;ad84	dd 5e 05	. ^ .
	add hl,de		;ad87	19		.
	ld (ix+004h),h		;ad88	dd 74 04	. t .
	ld (ix+005h),l		;ad8b	dd 75 05	. u .
	ret			;ad8e	c9		.
sub_ad8fh:
	ld iy,(l9789h)		;ad8f	fd 2a 89 97	. * . .
	ld hl,04081h		;ad93	21 81 40	! . @
	ld b,00ch		;ad96	06 0c		. .
lad98h:
	push bc			;ad98	c5		.
	push hl			;ad99	e5		.
	call sub_adach		;ad9a	cd ac ad	. . .
	pop hl			;ad9d	e1		.
	ld a,020h		;ad9e	3e 20		>  
	add a,l			;ada0	85		.
	ld l,a			;ada1	6f		o
	jr nc,lada8h		;ada2	30 04		0 .
	ld a,008h		;ada4	3e 08		> .
	add a,h			;ada6	84		.
	ld h,a			;ada7	67		g
lada8h:
	pop bc			;ada8	c1		.
	djnz lad98h		;ada9	10 ed		. .
	ret			;adab	c9		.
sub_adach:
	ld b,00fh		;adac	06 0f		. .
ladaeh:
	push bc			;adae	c5		.
	push hl			;adaf	e5		.
	call sub_adbch		;adb0	cd bc ad	. . .
	pop hl			;adb3	e1		.
	inc l			;adb4	2c		,
	inc l			;adb5	2c		,
	pop bc			;adb6	c1		.
	inc iy			;adb7	fd 23		. #
	djnz ladaeh		;adb9	10 f3		. .
	ret			;adbb	c9		.
sub_adbch:
	bit 7,(iy+000h)		;adbc	fd cb 00 7e	. . . ~
	ret nz			;adc0	c0		.
	bit 4,(iy+000h)		;adc1	fd cb 00 66	. . . f
	ret nz			;adc5	c0		.
	ld e,(ix+000h)		;adc6	dd 5e 00	. ^ .
	ld d,(ix+001h)		;adc9	dd 56 01	. V .
	ld (0addeh),sp		;adcc	ed 73 de ad	. s . .
	ex de,hl		;add0	eb		.
	ld sp,hl		;add1	f9		.
	ex de,hl		;add2	eb		.
	ld b,008h		;add3	06 08		. .
ladd5h:
	pop de			;add5	d1		.
	ld (hl),e		;add6	73		s
	inc l			;add7	2c		,
	ld (hl),d		;add8	72		r
	dec l			;add9	2d		-
	inc h			;adda	24		$
	djnz ladd5h		;addb	10 f8		. .
	ld sp,00000h		;addd	31 00 00	1 . .
	ret			;ade0	c9		.
sub_ade1h:
	ld iy,(l9789h)		;ade1	fd 2a 89 97	. * . .
	ld hl,0de01h		;ade5	21 01 de	! . .
	ld (laefbh),hl		;ade8	22 fb ae	" . .
	ld hl,0d7a2h		;adeb	21 a2 d7	! . .
	ld (laefdh),hl		;adee	22 fd ae	" . .
	ld b,00ch		;adf1	06 0c		. .
ladf3h:
	push bc			;adf3	c5		.
	push iy			;adf4	fd e5		. .
	call sub_ae13h		;adf6	cd 13 ae	. . .
	pop iy			;adf9	fd e1		. .
	call sub_ae2ah		;adfb	cd 2a ae	. * .
	ld hl,(laefbh)		;adfe	2a fb ae	* . .
	inc h			;ae01	24		$
	ld (laefbh),hl		;ae02	22 fb ae	" . .
	ld hl,(laefdh)		;ae05	2a fd ae	* . .
	ld de,00020h		;ae08	11 20 00	.   .
	add hl,de		;ae0b	19		.
	ld (laefdh),hl		;ae0c	22 fd ae	" . .
	pop bc			;ae0f	c1		.
	djnz ladf3h		;ae10	10 e1		. .
	ret			;ae12	c9		.
sub_ae13h:
	ld b,00fh		;ae13	06 0f		. .
	ld hl,(laefbh)		;ae15	2a fb ae	* . .
lae18h:
	push bc			;ae18	c5		.
	push hl			;ae19	e5		.
	bit 7,(iy+000h)		;ae1a	fd cb 00 7e	. . . ~
	call z,sub_ae82h	;ae1e	cc 82 ae	. . .
	pop hl			;ae21	e1		.
	inc l			;ae22	2c		,
	inc l			;ae23	2c		,
	inc iy			;ae24	fd 23		. #
	pop bc			;ae26	c1		.
	djnz lae18h		;ae27	10 ef		. .
	ret			;ae29	c9		.
sub_ae2ah:
	ld b,00fh		;ae2a	06 0f		. .
	ld hl,(laefdh)		;ae2c	2a fd ae	* . .
lae2fh:
	bit 7,(iy+000h)		;ae2f	fd cb 00 7e	. . . ~
	jr nz,lae42h		;ae33	20 0d		  .
	res 6,(hl)		;ae35	cb b6		. .
	inc l			;ae37	2c		,
	ld a,l			;ae38	7d		}
	cpl			;ae39	2f		/
	and 01fh		;ae3a	e6 1f		. .
	jr z,lae42h		;ae3c	28 04		( .
	res 6,(hl)		;ae3e	cb b6		. .
	jr lae43h		;ae40	18 01		. .
lae42h:
	inc l			;ae42	2c		,
lae43h:
	inc l			;ae43	2c		,
	inc iy			;ae44	fd 23		. #
	djnz lae2fh		;ae46	10 e7		. .
	ret			;ae48	c9		.
	ld hl,(laefbh)		;ae49	2a fb ae	* . .
	inc h			;ae4c	24		$
	inc l			;ae4d	2c		,
lae4eh:
	push bc			;ae4e	c5		.
	push hl			;ae4f	e5		.
	bit 7,(iy+000h)		;ae50	fd cb 00 7e	. . . ~
	call z,sub_ae60h	;ae54	cc 60 ae	. ` .
	pop hl			;ae57	e1		.
	inc l			;ae58	2c		,
	inc l			;ae59	2c		,
	inc iy			;ae5a	fd 23		. #
	pop bc			;ae5c	c1		.
	djnz lae4eh		;ae5d	10 ef		. .
	ret			;ae5f	c9		.
sub_ae60h:
	ld de,055aah		;ae60	11 aa 55	. . U
	ld c,l			;ae63	4d		M
	call sub_ae6fh		;ae64	cd 6f ae	. o .
	ld a,l			;ae67	7d		}
	and 01fh		;ae68	e6 1f		. .
	cp 01dh			;ae6a	fe 1d		. .
	ret z			;ae6c	c8		.
	ld l,c			;ae6d	69		i
	inc l			;ae6e	2c		,
sub_ae6fh:
	ld b,004h		;ae6f	06 04		. .
lae71h:
	ld a,(hl)		;ae71	7e		~
	and e			;ae72	a3		.
	ld (hl),a		;ae73	77		w
	ld a,l			;ae74	7d		}
	add a,020h		;ae75	c6 20		.  
	ld l,a			;ae77	6f		o
	ld a,(hl)		;ae78	7e		~
	and d			;ae79	a2		.
	ld (hl),a		;ae7a	77		w
	ld a,l			;ae7b	7d		}
	add a,020h		;ae7c	c6 20		.  
	ld l,a			;ae7e	6f		o
	djnz lae71h		;ae7f	10 f0		. .
	ret			;ae81	c9		.
sub_ae82h:
	ld (0aeb5h),sp		;ae82	ed 73 b5 ae	. s . .
	push hl			;ae86	e5		.
	ld de,0ffe0h		;ae87	11 e0 ff	. . .
	add hl,de		;ae8a	19		.
	ld (hl),000h		;ae8b	36 00		6 .
	inc l			;ae8d	2c		,
	ld (hl),000h		;ae8e	36 00		6 .
	pop hl			;ae90	e1		.
	ld a,l			;ae91	7d		}
	and 01fh		;ae92	e6 1f		. .
	dec a			;ae94	3d		=
	jr z,laea4h		;ae95	28 0d		( .
	push hl			;ae97	e5		.
	dec l			;ae98	2d		-
	ld de,00020h		;ae99	11 20 00	.   .
	ld b,008h		;ae9c	06 08		. .
lae9eh:
	res 0,(hl)		;ae9e	cb 86		. .
	add hl,de		;aea0	19		.
	djnz lae9eh		;aea1	10 fb		. .
	pop hl			;aea3	e1		.
laea4h:
	ld sp,laeffh		;aea4	31 ff ae	1 . .
	ld de,0001fh		;aea7	11 1f 00	. . .
	ld a,008h		;aeaa	3e 08		> .
laeach:
	pop bc			;aeac	c1		.
	ld (hl),c		;aead	71		q
	inc l			;aeae	2c		,
	ld (hl),b		;aeaf	70		p
	add hl,de		;aeb0	19		.
	dec a			;aeb1	3d		=
	jr nz,laeach		;aeb2	20 f8		  .
	ld sp,00000h		;aeb4	31 00 00	1 . .
	ld c,l			;aeb7	4d		M
	ld (hl),a		;aeb8	77		w
	inc l			;aeb9	2c		,
	ld (hl),a		;aeba	77		w
	ld a,l			;aebb	7d		}
	and 01fh		;aebc	e6 1f		. .
	cp 01eh			;aebe	fe 1e		. .
	jr z,laeceh		;aec0	28 0c		( .
	inc l			;aec2	2c		,
	ld de,0ffe0h		;aec3	11 e0 ff	. . .
	add hl,de		;aec6	19		.
	ld b,008h		;aec7	06 08		. .
laec9h:
	res 7,(hl)		;aec9	cb be		. .
	add hl,de		;aecb	19		.
	djnz laec9h		;aecc	10 fb		. .
laeceh:
	ld a,c			;aece	79		y
	and 01fh		;aecf	e6 1f		. .
	ld hl,(laefdh)		;aed1	2a fd ae	* . .
	add a,l			;aed4	85		.
	ld l,a			;aed5	6f		o
	ld de,0ffdeh		;aed6	11 de ff	. . .
	add hl,de		;aed9	19		.
	push hl			;aeda	e5		.
	ld a,(iy+000h)		;aedb	fd 7e 00	. ~ .
	and 00fh		;aede	e6 0f		. .
	ld hl,laeebh		;aee0	21 eb ae	! . .
	call sub_b5bbh		;aee3	cd bb b5	. . .
	ld b,(hl)		;aee6	46		F
	pop hl			;aee7	e1		.
	ld (hl),b		;aee8	70		p
	inc l			;aee9	2c		,
	ld (hl),b		;aeea	70		p
laeebh:
	ret			;aeeb	c9		.
	ld d,a			;aeec	57		W
	ld c,a			;aeed	4f		O
	ld e,a			;aeee	5f		_
	jr nz,laf61h		;aeef	20 70		  p
	ld b,a			;aef1	47		G
	ld d,a			;aef2	57		W
	ld e,a			;aef3	5f		_
	ld c,a			;aef4	4f		O
	nop			;aef5	00		.
	ld b,a			;aef6	47		G
	ld d,a			;aef7	57		W
	ld c,a			;aef8	4f		O
	ld e,a			;aef9	5f		_
	nop			;aefa	00		.
laefbh:
	nop			;aefb	00		.
	nop			;aefc	00		.
laefdh:
	nop			;aefd	00		.
	nop			;aefe	00		.
laeffh:
	rst 38h			;aeff	ff		.
	cp 080h			;af00	fe 80		. .
	nop			;af02	00		.
	add a,b			;af03	80		.
	nop			;af04	00		.
	add a,b			;af05	80		.
	nop			;af06	00		.
	add a,b			;af07	80		.
	nop			;af08	00		.
	add a,b			;af09	80		.
	nop			;af0a	00		.
	add a,b			;af0b	80		.
	nop			;af0c	00		.
	nop			;af0d	00		.
	nop			;af0e	00		.
	nop			;af0f	00		.
	ld (bc),a		;af10	02		.
	nop			;af11	00		.
	ld (bc),a		;af12	02		.
	nop			;af13	00		.
	ld (bc),a		;af14	02		.
	nop			;af15	00		.
	ld (bc),a		;af16	02		.
	nop			;af17	00		.
	ld (bc),a		;af18	02		.
	nop			;af19	00		.
	ld b,000h		;af1a	06 00		. .
	cp 000h			;af1c	fe 00		. .
	nop			;af1e	00		.
	nop			;af1f	00		.
	ld (bc),a		;af20	02		.
	nop			;af21	00		.
	ld (bc),a		;af22	02		.
	nop			;af23	00		.
	ld b,000h		;af24	06 00		. .
	ld b,000h		;af26	06 00		. .
	ld b,000h		;af28	06 00		. .
	ld c,00fh		;af2a	0e 0f		. .
	cp 000h			;af2c	fe 00		. .
	nop			;af2e	00		.
	nop			;af2f	00		.
	ld (bc),a		;af30	02		.
	nop			;af31	00		.
	ld (bc),a		;af32	02		.
	nop			;af33	00		.
	ld b,000h		;af34	06 00		. .
	ld b,000h		;af36	06 00		. .
	ld c,000h		;af38	0e 00		. .
	ld a,0ffh		;af3a	3e ff		> .
	cp 000h			;af3c	fe 00		. .
	nop			;af3e	00		.
laf3fh:
	rst 38h			;af3f	ff		.
	cp 0ffh			;af40	fe ff		. .
	cp 0ffh			;af42	fe ff		. .
	cp 0ffh			;af44	fe ff		. .
	cp 0ffh			;af46	fe ff		. .
	cp 0ffh			;af48	fe ff		. .
	cp 0ffh			;af4a	fe ff		. .
	cp 000h			;af4c	fe 00		. .
	nop			;af4e	00		.
	nop			;af4f	00		.
	ld (bc),a		;af50	02		.
	nop			;af51	00		.
	ld (bc),a		;af52	02		.
	nop			;af53	00		.
	ld (bc),a		;af54	02		.
	nop			;af55	00		.
	ld b,000h		;af56	06 00		. .
	ld b,000h		;af58	06 00		. .
	ld c,001h		;af5a	0e 01		. .
	cp 000h			;af5c	fe 00		. .
	nop			;af5e	00		.
	nop			;af5f	00		.
	ld (bc),a		;af60	02		.
laf61h:
	nop			;af61	00		.
	ld (bc),a		;af62	02		.
	nop			;af63	00		.
	ld b,000h		;af64	06 00		. .
	ld b,000h		;af66	06 00		. .
	ld b,000h		;af68	06 00		. .
	ld c,0ffh		;af6a	0e ff		. .
	cp 000h			;af6c	fe 00		. .
	nop			;af6e	00		.
laf6fh:
	rrca			;af6f	0f		.
	xor a			;af70	af		.
	ld c,a			;af71	4f		O
	xor a			;af72	af		.
	rra			;af73	1f		.
	xor a			;af74	af		.
	ld e,a			;af75	5f		_
	xor a			;af76	af		.
	cpl			;af77	2f		/
	xor a			;af78	af		.
	ccf			;af79	3f		?
	xor a			;af7a	af		.
	ccf			;af7b	3f		?
	xor a			;af7c	af		.
	rst 38h			;af7d	ff		.
	xor (hl)		;af7e	ae		.
	nop			;af7f	00		.
	nop			;af80	00		.
sub_af81h:
	ld iy,(l9789h)		;af81	fd 2a 89 97	. * . .
	xor a			;af85	af		.
	ld (0b2adh),a		;af86	32 ad b2	2 . .
	ld (l9653h),a		;af89	32 53 96	2 S .
	ld c,00ch		;af8c	0e 0c		. .
laf8eh:
	ld b,00fh		;af8e	06 0f		. .
laf90h:
	push bc			;af90	c5		.
	ld a,(iy+000h)		;af91	fd 7e 00	. ~ .
	and 0a0h		;af94	e6 a0		. .
	jr nz,lafa6h		;af96	20 0e		  .
	call sub_afc2h		;af98	cd c2 af	. . .
	call sub_974ah		;af9b	cd 4a 97	. J .
	ld a,(l9653h)		;af9e	3a 53 96	: S .
	xor 001h		;afa1	ee 01		. .
	ld (l9653h),a		;afa3	32 53 96	2 S .
lafa6h:
	push iy			;afa6	fd e5		. .
	call sub_c077h		;afa8	cd 77 c0	. w .
	pop iy			;afab	fd e1		. .
	ld d,003h		;afad	16 03		. .
	call sub_97d3h		;afaf	cd d3 97	. . .
	inc iy			;afb2	fd 23		. #
	pop bc			;afb4	c1		.
	djnz laf90h		;afb5	10 d9		. .
	ld a,(0b2adh)		;afb7	3a ad b2	: . .
	inc a			;afba	3c		<
	ld (0b2adh),a		;afbb	32 ad b2	2 . .
	dec c			;afbe	0d		.
	jr nz,laf8eh		;afbf	20 cd		  .
	ret			;afc1	c9		.
sub_afc2h:
	ld a,(0b2adh)		;afc2	3a ad b2	: . .
	add a,a			;afc5	87		.
	ld hl,lafe4h		;afc6	21 e4 af	! . .
	call sub_b5bbh		;afc9	cd bb b5	. . .
	ld b,(hl)		;afcc	46		F
	inc hl			;afcd	23		#
	ld c,(hl)		;afce	4e		N
	ld a,(iy+000h)		;afcf	fd 7e 00	. ~ .
	and 00fh		;afd2	e6 0f		. .
	cp 006h			;afd4	fe 06		. .
	jp c,l965dh		;afd6	da 5d 96	. ] .
	ld a,c			;afd9	79		y
	add a,c			;afda	81		.
	daa			;afdb	27		'
	ld c,a			;afdc	4f		O
	ld a,b			;afdd	78		x
	adc a,b			;afde	88		.
	daa			;afdf	27		'
	ld b,a			;afe0	47		G
	jp l965dh		;afe1	c3 5d 96	. ] .
lafe4h:
	ld bc,00120h		;afe4	01 20 01	.   .
	djnz lafeah		;afe7	10 01		. .
	nop			;afe9	00		.
lafeah:
	nop			;afea	00		.
	sub b			;afeb	90		.
	nop			;afec	00		.
	add a,b			;afed	80		.
	nop			;afee	00		.
	ld (hl),b		;afef	70		p
	nop			;aff0	00		.
	ld h,b			;aff1	60		`
	nop			;aff2	00		.
	ld d,b			;aff3	50		P
	nop			;aff4	00		.
	ld b,b			;aff5	40		@
	nop			;aff6	00		.
	jr nc,laff9h		;aff7	30 00		0 .
laff9h:
	jr nz,laffbh		;aff9	20 00		  .
laffbh:
	djnz $-79		;affb	10 af		. .
	ld (05cdbh),a		;affd	32 db 5c	2 . \
	ld a,(ix+004h)		;b000	dd 7e 04	. ~ .
	cp 080h			;b003	fe 80		. .
	ret nc			;b005	d0		.
	add a,(ix+00dh)		;b006	dd 86 0d	. . .
	cp 020h			;b009	fe 20		.  
	ret c			;b00b	d8		.
	ld iy,(l9789h)		;b00c	fd 2a 89 97	. * . .
	ld de,0000fh		;b010	11 0f 00	. . .
	ld h,d			;b013	62		b
	ld b,00ch		;b014	06 0c		. .
	ld c,020h		;b016	0e 20		.  
lb018h:
	ld a,c			;b018	79		y
	sub (ix+004h)		;b019	dd 96 04	. . .
	jr c,lb025h		;b01c	38 07		8 .
	sub (ix+00dh)		;b01e	dd 96 0d	. . .
	jr c,lb033h		;b021	38 10		8 .
	jr lb029h		;b023	18 04		. .
lb025h:
	add a,008h		;b025	c6 08		. .
	jr c,lb033h		;b027	38 0a		8 .
lb029h:
	add iy,de		;b029	fd 19		. .
	ld a,c			;b02b	79		y
	add a,008h		;b02c	c6 08		. .
	ld c,a			;b02e	4f		O
	inc h			;b02f	24		$
	djnz lb018h		;b030	10 e6		. .
	ret			;b032	c9		.
lb033h:
	ld a,h			;b033	7c		|
	ld (0b2adh),a		;b034	32 ad b2	2 . .
	ld a,(ix+004h)		;b037	dd 7e 04	. ~ .
	add a,(ix+00dh)		;b03a	dd 86 0d	. . .
	sub c			;b03d	91		.
	ld (0b088h),a		;b03e	32 88 b0	2 . .
	ld h,c			;b041	61		a
	ld a,(ix+002h)		;b042	dd 7e 02	. ~ .
	ld bc,01008h		;b045	01 08 10	. . .
	sub c			;b048	91		.
lb049h:
	sub b			;b049	90		.
	jr c,lb055h		;b04a	38 09		8 .
	inc iy			;b04c	fd 23		. #
	ld e,a			;b04e	5f		_
	ld a,c			;b04f	79		y
	add a,b			;b050	80		.
	ld c,a			;b051	4f		O
	ld a,e			;b052	7b		{
	jr lb049h		;b053	18 f4		. .
lb055h:
	ld (0b06ah),a		;b055	32 6a b0	2 j .
	ld l,c			;b058	69		i
	ld d,00fh		;b059	16 0f		. .
	bit 7,(iy+000h)		;b05b	fd cb 00 7e	. . . ~
	jr z,lb0aeh		;b05f	28 4d		( M
	ld a,l			;b061	7d		}
	cp 0e8h			;b062	fe e8		. .
	jr z,lb07fh		;b064	28 19		( .
	ld a,(ix+00ch)		;b066	dd 7e 0c	. ~ .
	add a,000h		;b069	c6 00		. .
	jr nc,lb07fh		;b06b	30 12		0 .
	set 7,d			;b06d	cb fa		. .
	ld e,l			;b06f	5d		]
	ld a,010h		;b070	3e 10		> .
	add a,l			;b072	85		.
	ld l,a			;b073	6f		o
	inc iy			;b074	fd 23		. #
	bit 7,(iy+000h)		;b076	fd cb 00 7e	. . . ~
	jr z,lb0aeh		;b07a	28 32		( 2
	ld l,e			;b07c	6b		k
	dec iy			;b07d	fd 2b		. +
lb07fh:
	ld a,h			;b07f	7c		|
	cp 078h			;b080	fe 78		. x
	ret nc			;b082	d0		.
	ld c,008h		;b083	0e 08		. .
	add a,c			;b085	81		.
	ld h,a			;b086	67		g
	ld a,000h		;b087	3e 00		> .
	sub c			;b089	91		.
	ret c			;b08a	d8		.
	ld b,000h		;b08b	06 00		. .
	ld c,00fh		;b08d	0e 0f		. .
	add iy,bc		;b08f	fd 09		. .
	push hl			;b091	e5		.
	pop hl			;b092	e1		.
	ld a,(0b2adh)		;b093	3a ad b2	: . .
	inc a			;b096	3c		<
	ld (0b2adh),a		;b097	32 ad b2	2 . .
	bit 7,(iy+000h)		;b09a	fd cb 00 7e	. . . ~
	jr z,lb0aeh		;b09e	28 0e		( .
	bit 7,d			;b0a0	cb 7a		. z
	ret z			;b0a2	c8		.
	ld a,l			;b0a3	7d		}
	add a,010h		;b0a4	c6 10		. .
	ld l,a			;b0a6	6f		o
	inc iy			;b0a7	fd 23		. #
	bit 7,(iy+000h)		;b0a9	fd cb 00 7e	. . . ~
	ret nz			;b0ad	c0		.
lb0aeh:
	ld (lb28fh+1),hl	;b0ae	22 90 b2	" . .
	push hl			;b0b1	e5		.
	ld l,(ix+002h)		;b0b2	dd 6e 02	. n .
	ld h,(ix+004h)		;b0b5	dd 66 04	. f .
	ld (0b1c4h),hl		;b0b8	22 c4 b1	" . .
	pop hl			;b0bb	e1		.
	res 7,d			;b0bc	cb ba		. .
	ld a,l			;b0be	7d		}
	cp 0e8h			;b0bf	fe e8		. .
	jr z,lb0c9h		;b0c1	28 06		( .
	bit 7,(iy+001h)		;b0c3	fd cb 01 7e	. . . ~
	jr nz,lb0cbh		;b0c7	20 02		  .
lb0c9h:
	res 1,d			;b0c9	cb 8a		. .
lb0cbh:
	cp 008h			;b0cb	fe 08		. .
	jr z,lb0d5h		;b0cd	28 06		( .
	bit 7,(iy-001h)		;b0cf	fd cb ff 7e	. . . ~
	jr nz,lb0d7h		;b0d3	20 02		  .
lb0d5h:
	res 0,d			;b0d5	cb 82		. .
lb0d7h:
	ld a,h			;b0d7	7c		|
	cp 021h			;b0d8	fe 21		. !
	jr c,lb0e4h		;b0da	38 08		8 .
	bit 7,(iy-00fh)		;b0dc	fd cb f1 7e	. . . ~
	jr nz,lb0e4h		;b0e0	20 02		  .
	res 2,d			;b0e2	cb 92		. .
lb0e4h:
	cp 078h			;b0e4	fe 78		. x
	jr nc,lb0f0h		;b0e6	30 08		0 .
	bit 7,(iy+00fh)		;b0e8	fd cb 0f 7e	. . . ~
	jr nz,lb0f0h		;b0ec	20 02		  .
	res 3,d			;b0ee	cb 9a		. .
lb0f0h:
	ld a,d			;b0f0	7a		z
	ld (0b293h),a		;b0f1	32 93 b2	2 . .
	ld a,(ix+000h)		;b0f4	dd 7e 00	. ~ .
	and 03fh		;b0f7	e6 3f		. ?
	cp 005h			;b0f9	fe 05		. .
	jp z,lb197h		;b0fb	ca 97 b1	. . .
	ld a,(ix+006h)		;b0fe	dd 7e 06	. ~ .
	ld (0b28eh),a		;b101	32 8e b2	2 . .
	cp 020h			;b104	fe 20		.  
	jr nc,lb10ch		;b106	30 04		0 .
	res 3,d			;b108	cb 9a		. .
	jr lb10eh		;b10a	18 02		. .
lb10ch:
	res 2,d			;b10c	cb 92		. .
lb10eh:
	add a,010h		;b10e	c6 10		. .
	and 03fh		;b110	e6 3f		. ?
	cp 020h			;b112	fe 20		.  
	jr nc,lb11ah		;b114	30 04		0 .
	res 1,d			;b116	cb 8a		. .
	jr lb11ch		;b118	18 02		. .
lb11ah:
	res 0,d			;b11a	cb 82		. .
lb11ch:
	ld a,d			;b11c	7a		z
lb11dh:
	srl a			;b11d	cb 3f		. ?
	ld b,01fh		;b11f	06 1f		. .
	jr nc,lb128h		;b121	30 05		0 .
	jp z,lb171h		;b123	ca 71 b1	. q .
	jr lb13bh		;b126	18 13		. .
lb128h:
	srl a			;b128	cb 3f		. ?
	jr nc,lb131h		;b12a	30 05		0 .
	jp z,lb17eh		;b12c	ca 7e b1	. ~ .
	jr lb13bh		;b12f	18 0a		. .
lb131h:
	ld b,03fh		;b131	06 3f		. ?
	srl a			;b133	cb 3f		. ?
	jp nc,lb197h		;b135	d2 97 b1	. . .
	jp z,lb18ah		;b138	ca 8a b1	. . .
lb13bh:
	bit 0,d			;b13b	cb 42		. B
	jr z,lb148h		;b13d	28 09		( .
	ld a,(ix+00ch)		;b13f	dd 7e 0c	. ~ .
	add a,(ix+002h)		;b142	dd 86 02	. . .
	sub l			;b145	95		.
	jr lb14eh		;b146	18 06		. .
lb148h:
	ld a,l			;b148	7d		}
	add a,010h		;b149	c6 10		. .
	sub (ix+002h)		;b14b	dd 96 02	. . .
lb14eh:
	ld c,a			;b14e	4f		O
	bit 2,d			;b14f	cb 52		. R
	jr z,lb15ch		;b151	28 09		( .
	ld a,(ix+00dh)		;b153	dd 7e 0d	. ~ .
	add a,(ix+004h)		;b156	dd 86 04	. . .
	sub h			;b159	94		.
	jr lb162h		;b15a	18 06		. .
lb15ch:
	ld a,h			;b15c	7c		|
	add a,008h		;b15d	c6 08		. .
	sub (ix+004h)		;b15f	dd 96 04	. . .
lb162h:
	ld e,d			;b162	5a		Z
	cp c			;b163	b9		.
	res 2,d			;b164	cb 92		. .
	res 3,d			;b166	cb 9a		. .
	jp nc,lb11ch		;b168	d2 1c b1	. . .
	ld a,e			;b16b	7b		{
	and 00ch		;b16c	e6 0c		. .
	jp lb11dh		;b16e	c3 1d b1	. . .
lb171h:
	ld a,l			;b171	7d		}
	sub (ix+00ch)		;b172	dd 96 0c	. . .
	ld (ix+002h),a		;b175	dd 77 02	. w .
	call sub_aceeh		;b178	cd ee ac	. . .
	jp lb1a3h		;b17b	c3 a3 b1	. . .
lb17eh:
	ld a,l			;b17e	7d		}
	add a,010h		;b17f	c6 10		. .
	ld (ix+002h),a		;b181	dd 77 02	. w .
	call sub_aceeh		;b184	cd ee ac	. . .
	jp lb1a3h		;b187	c3 a3 b1	. . .
lb18ah:
	ld a,h			;b18a	7c		|
	sub (ix+00dh)		;b18b	dd 96 0d	. . .
	ld (ix+004h),a		;b18e	dd 77 04	. w .
	call sub_aceeh		;b191	cd ee ac	. . .
	jp lb1a3h		;b194	c3 a3 b1	. . .
lb197h:
	ld a,h			;b197	7c		|
	add a,008h		;b198	c6 08		. .
	ld (ix+004h),a		;b19a	dd 77 04	. w .
	call sub_aceeh		;b19d	cd ee ac	. . .
	jp lb1a3h		;b1a0	c3 a3 b1	. . .
lb1a3h:
	ld a,001h		;b1a3	3e 01		> .
	ld (05cdbh),a		;b1a5	32 db 5c	2 . \
	ld a,(ix+000h)		;b1a8	dd 7e 00	. ~ .
	and 07fh		;b1ab	e6 7f		. .
	cp 002h			;b1ad	fe 02		. .
	jr z,lb1e7h		;b1af	28 36		( 6
	cp 005h			;b1b1	fe 05		. .
	jr z,lb1cdh		;b1b3	28 18		( .
	and 0feh		;b1b5	e6 fe		. .
	cp 008h			;b1b7	fe 08		. .
	ret nz			;b1b9	c0		.
	ld l,(ix+002h)		;b1ba	dd 6e 02	. n .
	ld h,(ix+004h)		;b1bd	dd 66 04	. f .
	ld (laa7bh),hl		;b1c0	22 7b aa	" { .
	ld hl,00000h		;b1c3	21 00 00	! . .
	ld (ix+002h),l		;b1c6	dd 75 02	. u .
	ld (ix+004h),h		;b1c9	dd 74 04	. t .
	ret			;b1cc	c9		.
lb1cdh:
	ld (ix+001h),002h	;b1cd	dd 36 01 02	. 6 . .
	ld (ix+009h),006h	;b1d1	dd 36 09 06	. 6 . .
	ld a,(ix+002h)		;b1d5	dd 7e 02	. ~ .
	and 0f8h		;b1d8	e6 f8		. .
	ld (ix+002h),a		;b1da	dd 77 02	. w .
	ld (ix+012h),050h	;b1dd	dd 36 12 50	. 6 . P
	ld (ix+013h),050h	;b1e1	dd 36 13 50	. 6 . P
	jr lb1fbh		;b1e5	18 14		. .
lb1e7h:
	bit 5,(iy+000h)		;b1e7	fd cb 00 6e	. . . n
	jr nz,lb20ch		;b1eb	20 1f		  .
	ld a,(l9b68h)		;b1ed	3a 68 9b	: h .
	cp 007h			;b1f0	fe 07		. .
	jr z,lb259h		;b1f2	28 65		( e
	ld a,(l9b52h)		;b1f4	3a 52 9b	: R .
	cp 007h			;b1f7	fe 07		. .
	jr z,lb259h		;b1f9	28 5e		( ^
lb1fbh:
	bit 4,(iy+000h)		;b1fb	fd cb 00 66	. . . f
	jp nz,lb259h		;b1ff	c2 59 b2	. Y .
	bit 5,(iy+000h)		;b202	fd cb 00 6e	. . . n
	jr nz,lb20ch		;b206	20 04		  .
	set 4,(iy+000h)		;b208	fd cb 00 e6	. . . .
lb20ch:
	ld hl,(lb28fh+1)	;b20c	2a 90 b2	* . .
	ld de,00007h		;b20f	11 07 00	. . .
	push ix			;b212	dd e5		. .
	ld ix,lb6f4h		;b214	dd 21 f4 b6	. ! . .
	ld b,005h		;b218	06 05		. .
lb21ah:
	ld a,(ix+000h)		;b21a	dd 7e 00	. ~ .
	and a			;b21d	a7		.
	jr z,lb226h		;b21e	28 06		( .
	add ix,de		;b220	dd 19		. .
	djnz lb21ah		;b222	10 f6		. .
	jr lb246h		;b224	18 20		.  
lb226h:
	push hl			;b226	e5		.
	call sub_b57dh		;b227	cd 7d b5	. } .
	ld (ix+001h),l		;b22a	dd 75 01	. u .
	ld (ix+002h),h		;b22d	dd 74 02	. t .
	pop hl			;b230	e1		.
	call sub_c03dh		;b231	cd 3d c0	. = .
	ld (ix+003h),l		;b234	dd 75 03	. u .
	ld (ix+004h),h		;b237	dd 74 04	. t .
	inc (ix+000h)		;b23a	dd 34 00	. 4 .
	push iy			;b23d	fd e5		. .
	pop de			;b23f	d1		.
	ld (ix+005h),e		;b240	dd 73 05	. s .
	ld (ix+006h),d		;b243	dd 72 06	. r .
lb246h:
	ld ix,lc0d4h		;b246	dd 21 d4 c0	. ! . .
	ld (ix+000h),002h	;b24a	dd 36 00 02	. 6 . .
	ld (ix+001h),009h	;b24e	dd 36 01 09	. 6 . .
	ld (ix+002h),0b0h	;b252	dd 36 02 b0	. 6 . .
	pop ix			;b256	dd e1		. .
	ret			;b258	c9		.
lb259h:
	ld a,(lb7e9h)		;b259	3a e9 b7	: . .
	dec a			;b25c	3d		=
	ld (lb7e9h),a		;b25d	32 e9 b7	2 . .
	call sub_afc2h		;b260	cd c2 af	. . .
	ld a,(ix+000h)		;b263	dd 7e 00	. ~ .
	and 07fh		;b266	e6 7f		. .
	cp 002h			;b268	fe 02		. .
	jr nz,lb28fh		;b26a	20 23		  #
	ld a,(ix+012h)		;b26c	dd 7e 12	. ~ .
	and 080h		;b26f	e6 80		. .
	ld (ix+012h),a		;b271	dd 77 12	. w .
	ld a,(l9b68h)		;b274	3a 68 9b	: h .
	cp 007h			;b277	fe 07		. .
	jr z,lb282h		;b279	28 07		( .
	ld a,(l9b52h)		;b27b	3a 52 9b	: R .
	cp 007h			;b27e	fe 07		. .
	jr nz,lb28fh		;b280	20 0d		  .
lb282h:
	ld hl,(0b1c4h)		;b282	2a c4 b1	* . .
	ld (ix+002h),l		;b285	dd 75 02	. u .
	ld (ix+004h),h		;b288	dd 74 04	. t .
	ld (ix+006h),000h	;b28b	dd 36 06 00	. 6 . .
lb28fh:
	ld hl,00000h		;b28f	21 00 00	! . .
	ld a,000h		;b292	3e 00		> .
	ld bc,00208h		;b294	01 08 02	. . .
	rra			;b297	1f		.
	jr nc,lb29ch		;b298	30 02		0 .
	dec l			;b29a	2d		-
	inc b			;b29b	04		.
lb29ch:
	rra			;b29c	1f		.
	jr nc,lb2a0h		;b29d	30 01		0 .
	inc b			;b29f	04		.
lb2a0h:
	rra			;b2a0	1f		.
	jr nc,lb2a5h		;b2a1	30 02		0 .
	dec h			;b2a3	25		%
	inc c			;b2a4	0c		.
lb2a5h:
	rra			;b2a5	1f		.
	jr nc,lb2a9h		;b2a6	30 01		0 .
	inc c			;b2a8	0c		.
lb2a9h:
	ld de,0ec02h		;b2a9	11 02 ec	. . .
	ld a,000h		;b2ac	3e 00		> .
	and 001h		;b2ae	e6 01		. .
	add a,d			;b2b0	82		.
	ld d,a			;b2b1	57		W
	push ix			;b2b2	dd e5		. .
	ld a,(l8d49h)		;b2b4	3a 49 8d	: I .
	and 00fh		;b2b7	e6 0f		. .
	cp 005h			;b2b9	fe 05		. .
	push ix			;b2bb	dd e5		. .
	call c,sub_9d5ah	;b2bd	dc 5a 9d	. Z .
	pop ix			;b2c0	dd e1		. .
	ld a,(ix+000h)		;b2c2	dd 7e 00	. ~ .
	and 07fh		;b2c5	e6 7f		. .
	cp 005h			;b2c7	fe 05		. .
	jr z,lb2d8h		;b2c9	28 0d		( .
	exx			;b2cb	d9		.
	call sub_c064h		;b2cc	cd 64 c0	. d .
	ld (ix+000h),001h	;b2cf	dd 36 00 01	. 6 . .
	ld (ix+001h),004h	;b2d3	dd 36 01 04	. 6 . .
	exx			;b2d7	d9		.
lb2d8h:
	ld ix,l9c08h		;b2d8	dd 21 08 9c	. ! . .
	ld (ix+001h),l		;b2dc	dd 75 01	. u .
	ld (ix+000h),h		;b2df	dd 74 00	. t .
	ld (ix+002h),b		;b2e2	dd 70 02	. p .
	ld (ix+003h),c		;b2e5	dd 71 03	. q .
	push bc			;b2e8	c5		.
	call sub_c03dh		;b2e9	cd 3d c0	. = .
	pop bc			;b2ec	c1		.
	ld a,(l9c24h)		;b2ed	3a 24 9c	: $ .
	inc a			;b2f0	3c		<
	ld (l9c24h),a		;b2f1	32 24 9c	2 $ .
	bit 0,(ix+000h)		;b2f4	dd cb 00 46	. . . F
	jr z,lb2ffh		;b2f8	28 05		( .
	ld a,e			;b2fa	7b		{
	sub 020h		;b2fb	d6 20		.  
	ld e,a			;b2fd	5f		_
	dec d			;b2fe	15		.
lb2ffh:
	ld a,l			;b2ff	7d		}
	and 001h		;b300	e6 01		. .
	add a,e			;b302	83		.
	ld e,a			;b303	5f		_
	ld (0b3e0h),hl		;b304	22 e0 b3	" . .
	ex de,hl		;b307	eb		.
	ld a,c			;b308	79		y
	cp 008h			;b309	fe 08		. .
	jr z,lb329h		;b30b	28 1c		( .
	bit 0,(ix+000h)		;b30d	dd cb 00 46	. . . F
	jr z,lb329h		;b311	28 16		( .
	push de			;b313	d5		.
	push hl			;b314	e5		.
	set 0,l			;b315	cb c5		. .
	set 0,e			;b317	cb c3		. .
	ldi			;b319	ed a0		. .
	ldi			;b31b	ed a0		. .
	pop hl			;b31d	e1		.
	pop de			;b31e	d1		.
	ld a,020h		;b31f	3e 20		>  
	add a,e			;b321	83		.
	ld e,a			;b322	5f		_
	inc d			;b323	14		.
	ld a,020h		;b324	3e 20		>  
	add a,l			;b326	85		.
	ld l,a			;b327	6f		o
	inc h			;b328	24		$
lb329h:
	ld (0b3e3h),de		;b329	ed 53 e3 b3	. S . .
	push de			;b32d	d5		.
	ld a,020h		;b32e	3e 20		>  
	sub b			;b330	90		.
	ld (0b342h),a		;b331	32 42 b3	2 B .
	ld a,b			;b334	78		x
	ld (0b33eh),a		;b335	32 3e b3	2 > .
	ld a,008h		;b338	3e 08		> .
	ld b,000h		;b33a	06 00		. .
lb33ch:
	ex af,af'		;b33c	08		.
	ld c,000h		;b33d	0e 00		. .
	ldir			;b33f	ed b0		. .
	ld c,000h		;b341	0e 00		. .
	add hl,bc		;b343	09		.
	ex de,hl		;b344	eb		.
	add hl,bc		;b345	09		.
	ex de,hl		;b346	eb		.
	ex af,af'		;b347	08		.
	dec a			;b348	3d		=
	jr nz,lb33ch		;b349	20 f1		  .
	push de			;b34b	d5		.
	ld a,(ix+003h)		;b34c	dd 7e 03	. ~ .
	cp 008h			;b34f	fe 08		. .
	jr z,lb369h		;b351	28 16		( .
	cp 00ah			;b353	fe 0a		. .
	jr z,lb35dh		;b355	28 06		( .
	bit 0,(ix+000h)		;b357	dd cb 00 46	. . . F
	jr nz,lb369h		;b35b	20 0c		  .
lb35dh:
	bit 3,(ix+001h)		;b35d	dd cb 01 5e	. . . ^
	jr nz,lb365h		;b361	20 02		  .
	inc l			;b363	2c		,
	inc e			;b364	1c		.
lb365h:
	ldi			;b365	ed a0		. .
	ldi			;b367	ed a0		. .
lb369h:
	ld a,(ix+001h)		;b369	dd 7e 01	. ~ .
	cp 0e0h			;b36c	fe e0		. .
	jr c,lb377h		;b36e	38 07		8 .
	ld hl,0e05eh		;b370	21 5e e0	! ^ .
	ld c,0feh		;b373	0e fe		. .
	jr lb380h		;b375	18 09		. .
lb377h:
	cp 010h			;b377	fe 10		. .
	jr nc,lb39ah		;b379	30 1f		0 .
	ld hl,0e041h		;b37b	21 41 e0	! A .
	ld c,07fh		;b37e	0e 7f		. .
lb380h:
	ld b,01ch		;b380	06 1c		. .
	ld de,00020h		;b382	11 20 00	.   .
lb385h:
	ld a,(hl)		;b385	7e		~
	and c			;b386	a1		.
	ld (hl),a		;b387	77		w
	add hl,de		;b388	19		.
	djnz lb385h		;b389	10 fa		. .
	ld de,00380h		;b38b	11 80 03	. . .
	add hl,de		;b38e	19		.
	ld de,00020h		;b38f	11 20 00	.   .
	ld b,018h		;b392	06 18		. .
lb394h:
	ld a,(hl)		;b394	7e		~
	and c			;b395	a1		.
	ld (hl),a		;b396	77		w
	add hl,de		;b397	19		.
	djnz lb394h		;b398	10 fa		. .
lb39ah:
	ld de,(lb28fh+1)	;b39a	ed 5b 90 b2	. [ . .
	pop hl			;b39e	e1		.
	ld b,h			;b39f	44		D
	ld c,l			;b3a0	4d		M
	ld a,d			;b3a1	7a		z
	cp 078h			;b3a2	fe 78		. x
	jr z,lb3dah		;b3a4	28 34		( 4
	dec b			;b3a6	05		.
	ld a,c			;b3a7	79		y
	sub 020h		;b3a8	d6 20		.  
	ld c,a			;b3aa	4f		O
	set 0,l			;b3ab	cb c5		. .
	ld a,e			;b3ad	7b		{
	cp 008h			;b3ae	fe 08		. .
	jr z,lb3c1h		;b3b0	28 0f		( .
	bit 7,(iy+00eh)		;b3b2	fd cb 0e 7e	. . . ~
	jr nz,lb3c1h		;b3b6	20 09		  .
	res 7,(hl)		;b3b8	cb be		. .
	bit 0,c			;b3ba	cb 41		. A
	jr nz,lb3c1h		;b3bc	20 03		  .
	xor a			;b3be	af		.
	ld (bc),a		;b3bf	02		.
	ld a,e			;b3c0	7b		{
lb3c1h:
	cp 0e0h			;b3c1	fe e0		. .
	jr nc,lb3dah		;b3c3	30 15		0 .
	bit 7,(iy+010h)		;b3c5	fd cb 10 7e	. . . ~
	jr nz,lb3dah		;b3c9	20 0f		  .
	inc l			;b3cb	2c		,
	ld a,(ix+002h)		;b3cc	dd 7e 02	. ~ .
	dec a			;b3cf	3d		=
	add a,c			;b3d0	81		.
	ld c,a			;b3d1	4f		O
	res 0,(hl)		;b3d2	cb 86		. .
	and 001h		;b3d4	e6 01		. .
	jr z,lb3dah		;b3d6	28 02		( .
	xor a			;b3d8	af		.
	ld (bc),a		;b3d9	02		.
lb3dah:
	ld a,d			;b3da	7a		z
	cp 020h			;b3db	fe 20		.  
	jr z,lb414h		;b3dd	28 35		( 5
	ld hl,00000h		;b3df	21 00 00	! . .
	ld bc,00000h		;b3e2	01 00 00	. . .
	set 0,l			;b3e5	cb c5		. .
	ld a,e			;b3e7	7b		{
	cp 008h			;b3e8	fe 08		. .
	jr z,lb3fbh		;b3ea	28 0f		( .
	bit 7,(iy-010h)		;b3ec	fd cb f0 7e	. . . ~
	jr nz,lb3fbh		;b3f0	20 09		  .
	res 7,(hl)		;b3f2	cb be		. .
	bit 0,c			;b3f4	cb 41		. A
	jr nz,lb3fbh		;b3f6	20 03		  .
	xor a			;b3f8	af		.
	ld (bc),a		;b3f9	02		.
	ld a,e			;b3fa	7b		{
lb3fbh:
	cp 0e0h			;b3fb	fe e0		. .
	jr nc,lb414h		;b3fd	30 15		0 .
	bit 7,(iy-00eh)		;b3ff	fd cb f2 7e	. . . ~
	jr nz,lb414h		;b403	20 0f		  .
	inc l			;b405	2c		,
	res 0,(hl)		;b406	cb 86		. .
	ld a,(ix+002h)		;b408	dd 7e 02	. ~ .
	dec a			;b40b	3d		=
	add a,c			;b40c	81		.
	ld c,a			;b40d	4f		O
	and 001h		;b40e	e6 01		. .
	jr z,lb414h		;b410	28 02		( .
	xor a			;b412	af		.
	ld (bc),a		;b413	02		.
lb414h:
	pop hl			;b414	e1		.
	ld de,00020h		;b415	11 20 00	.   .
	ld a,(ix+001h)		;b418	dd 7e 01	. ~ .
	cp 008h			;b41b	fe 08		. .
	jr z,lb42ch		;b41d	28 0d		( .
	bit 3,a			;b41f	cb 5f		. _
	jr z,lb42ch		;b421	28 09		( .
	push hl			;b423	e5		.
	ld b,008h		;b424	06 08		. .
lb426h:
	res 7,(hl)		;b426	cb be		. .
	add hl,de		;b428	19		.
	djnz lb426h		;b429	10 fb		. .
	pop hl			;b42b	e1		.
lb42ch:
	ld a,(ix+001h)		;b42c	dd 7e 01	. ~ .
	cp 0e0h			;b42f	fe e0		. .
	jr nc,lb454h		;b431	30 21		0 !
	ld b,(ix+002h)		;b433	dd 46 02	. F .
	sla b			;b436	cb 20		.  
	sla b			;b438	cb 20		.  
	sla b			;b43a	cb 20		.  
	add a,b			;b43c	80		.
	cp 0f8h			;b43d	fe f8		. .
	jr z,lb454h		;b43f	28 13		( .
	and 008h		;b441	e6 08		. .
	jr z,lb454h		;b443	28 0f		( .
	push hl			;b445	e5		.
	ld a,(ix+002h)		;b446	dd 7e 02	. ~ .
	dec a			;b449	3d		=
	add a,l			;b44a	85		.
	ld l,a			;b44b	6f		o
	ld b,008h		;b44c	06 08		. .
lb44eh:
	res 0,(hl)		;b44e	cb 86		. .
	add hl,de		;b450	19		.
	djnz lb44eh		;b451	10 fb		. .
	pop hl			;b453	e1		.
lb454h:
	set 0,l			;b454	cb c5		. .
	ld a,(0b293h)		;b456	3a 93 b2	: . .
	bit 2,a			;b459	cb 57		. W
	jr nz,lb461h		;b45b	20 04		  .
	ld (hl),d		;b45d	72		r
	inc l			;b45e	2c		,
	ld (hl),d		;b45f	72		r
	dec l			;b460	2d		-
lb461h:
	and 008h		;b461	e6 08		. .
	jr nz,lb46ch		;b463	20 07		  .
	ld a,l			;b465	7d		}
	add a,0e0h		;b466	c6 e0		. .
	ld l,a			;b468	6f		o
	ld (hl),d		;b469	72		r
	inc l			;b46a	2c		,
	ld (hl),d		;b46b	72		r
lb46ch:
	ld de,00004h		;b46c	11 04 00	. . .
	add ix,de		;b46f	dd 19		. .
	ld (lb2d8h+2),ix	;b471	dd 22 da b2	. " . .
	pop ix			;b475	dd e1		. .
	ld hl,(lb28fh+1)	;b477	2a 90 b2	* . .
	call sub_c051h		;b47a	cd 51 c0	. Q .
	push hl			;b47d	e5		.
	push hl			;b47e	e5		.
	ld de,(0d742h)		;b47f	ed 5b 42 d7	. [ B .
	ld a,(lb28fh+1)		;b483	3a 90 b2	: . .
	cp 008h			;b486	fe 08		. .
	res 6,e			;b488	cb b3		. .
	jr z,lb48eh		;b48a	28 02		( .
	set 6,e			;b48c	cb f3		. .
lb48eh:
	ld a,(lb28fh+2)		;b48e	3a 91 b2	: . .
	cp 020h			;b491	fe 20		.  
	jr z,lb4a5h		;b493	28 10		( .
	bit 7,(iy-010h)		;b495	fd cb f0 7e	. . . ~
	jr nz,lb49dh		;b499	20 02		  .
	res 6,e			;b49b	cb b3		. .
lb49dh:
	bit 7,(iy-00fh)		;b49d	fd cb f1 7e	. . . ~
	jr nz,lb4a5h		;b4a1	20 02		  .
	res 6,d			;b4a3	cb b2		. .
lb4a5h:
	ld (hl),e		;b4a5	73		s
	inc l			;b4a6	2c		,
	ld (hl),d		;b4a7	72		r
	ld de,00020h		;b4a8	11 20 00	.   .
	add hl,de		;b4ab	19		.
	ld a,(lb28fh+2)		;b4ac	3a 91 b2	: . .
	cp 078h			;b4af	fe 78		. x
	jr nz,lb4bah		;b4b1	20 07		  .
	set 6,(hl)		;b4b3	cb f6		. .
	inc l			;b4b5	2c		,
	set 6,(hl)		;b4b6	cb f6		. .
	jr lb4d2h		;b4b8	18 18		. .
lb4bah:
	bit 7,(iy+00fh)		;b4ba	fd cb 0f 7e	. . . ~
	jr z,lb4c2h		;b4be	28 02		( .
	set 6,(hl)		;b4c0	cb f6		. .
lb4c2h:
	inc l			;b4c2	2c		,
	ld a,(lb28fh+1)		;b4c3	3a 90 b2	: . .
	cp 0e8h			;b4c6	fe e8		. .
	jr z,lb4d2h		;b4c8	28 08		( .
	bit 7,(iy+010h)		;b4ca	fd cb 10 7e	. . . ~
	jr z,lb4d2h		;b4ce	28 02		( .
	set 6,(hl)		;b4d0	cb f6		. .
lb4d2h:
	pop hl			;b4d2	e1		.
	pop de			;b4d3	d1		.
	ld a,d			;b4d4	7a		z
	sub 07fh		;b4d5	d6 7f		. .
	ld d,a			;b4d7	57		W
	ldi			;b4d8	ed a0		. .
	ldi			;b4da	ed a0		. .
	ld bc,0001fh		;b4dc	01 1f 00	. . .
	add hl,bc		;b4df	09		.
	ex de,hl		;b4e0	eb		.
	add hl,bc		;b4e1	09		.
	ex de,hl		;b4e2	eb		.
	ldi			;b4e3	ed a0		. .
	ldi			;b4e5	ed a0		. .
	set 7,(iy+000h)		;b4e7	fd cb 00 fe	. . . .
	ret			;b4eb	c9		.
lb4ech:
	ex de,hl		;b4ec	eb		.
	ld e,(hl)		;b4ed	5e		^
	inc hl			;b4ee	23		#
	ld d,(hl)		;b4ef	56		V
	inc hl			;b4f0	23		#
	ld a,(hl)		;b4f1	7e		~
	ld (lb618h),a		;b4f2	32 18 b6	2 . .
	inc hl			;b4f5	23		#
	ld b,(hl)		;b4f6	46		F
	inc hl			;b4f7	23		#
	xor a			;b4f8	af		.
	ld (lb54fh+1),a		;b4f9	32 50 b5	2 P .
	inc a			;b4fc	3c		<
	ld (lb617h),a		;b4fd	32 17 b6	2 . .
	ld a,b			;b500	78		x
	and 03fh		;b501	e6 3f		. ?
	ld (lb616h),a		;b503	32 16 b6	2 . .
	bit 7,b			;b506	cb 78		. x
	jr z,lb51ah		;b508	28 10		( .
	bit 6,b			;b50a	cb 70		. p
	jr nz,lb51ah		;b50c	20 0c		  .
	ld a,00ch		;b50e	3e 0c		> .
	ld (lb54fh+1),a		;b510	32 50 b5	2 P .
	ld a,002h		;b513	3e 02		> .
	ld (lb617h),a		;b515	32 17 b6	2 . .
	jr lb51ah		;b518	18 00		. .
lb51ah:
	ex de,hl		;b51a	eb		.
	push bc			;b51b	c5		.
	push de			;b51c	d5		.
	ld a,h			;b51d	7c		|
	sub 003h		;b51e	d6 03		. .
	ld h,a			;b520	67		g
	call sub_b619h		;b521	cd 19 b6	. . .
	pop de			;b524	d1		.
	pop bc			;b525	c1		.
	ld a,h			;b526	7c		|
	add a,003h		;b527	c6 03		. .
	ld h,a			;b529	67		g
	call sub_b57dh		;b52a	cd 7d b5	. } .
	ld a,b			;b52d	78		x
	and 03fh		;b52e	e6 3f		. ?
	ld b,a			;b530	47		G
lb531h:
	push bc			;b531	c5		.
	call sub_b53ah		;b532	cd 3a b5	. : .
	pop bc			;b535	c1		.
	inc de			;b536	13		.
	djnz lb531h		;b537	10 f8		. .
	ret			;b539	c9		.
sub_b53ah:
	push de			;b53a	d5		.
	ld a,(de)		;b53b	1a		.
	ex de,hl		;b53c	eb		.
	push de			;b53d	d5		.
	ld l,a			;b53e	6f		o
	ld h,000h		;b53f	26 00		& .
	add hl,hl		;b541	29		)
	ld e,l			;b542	5d		]
	ld d,h			;b543	54		T
	add hl,hl		;b544	29		)
	add hl,de		;b545	19		.
	ld de,06a1ah		;b546	11 1a 6a	. . j
	add hl,de		;b549	19		.
	pop de			;b54a	d1		.
	ex de,hl		;b54b	eb		.
	push hl			;b54c	e5		.
	ld b,006h		;b54d	06 06		. .
lb54fh:
	jr lb54fh		;b54f	18 fe		. .
lb551h:
	ld a,(de)		;b551	1a		.
	ld (hl),a		;b552	77		w
	dec de			;b553	1b		.
	call sub_b56eh		;b554	cd 6e b5	. n .
	djnz lb551h		;b557	10 f8		. .
	pop hl			;b559	e1		.
	inc l			;b55a	2c		,
	pop de			;b55b	d1		.
	ret			;b55c	c9		.
lb55dh:
	ld a,(de)		;b55d	1a		.
	ld (hl),a		;b55e	77		w
	call sub_b56eh		;b55f	cd 6e b5	. n .
	ld a,(de)		;b562	1a		.
	ld (hl),a		;b563	77		w
	call sub_b56eh		;b564	cd 6e b5	. n .
	dec de			;b567	1b		.
	djnz lb55dh		;b568	10 f3		. .
	pop hl			;b56a	e1		.
	inc l			;b56b	2c		,
	pop de			;b56c	d1		.
	ret			;b56d	c9		.
sub_b56eh:
	ld a,h			;b56e	7c		|
	dec h			;b56f	25		%
	and 007h		;b570	e6 07		. .
	ret nz			;b572	c0		.
	ld a,l			;b573	7d		}
	sub 020h		;b574	d6 20		.  
	ld l,a			;b576	6f		o
	ret c			;b577	d8		.
	ld a,h			;b578	7c		|
	add a,008h		;b579	c6 08		. .
	ld h,a			;b57b	67		g
	ret			;b57c	c9		.
sub_b57dh:
	ld a,l			;b57d	7d		}
	rrc a			;b57e	cb 0f		. .
	rrc a			;b580	cb 0f		. .
	rrc a			;b582	cb 0f		. .
	and 01fh		;b584	e6 1f		. .
	ld l,a			;b586	6f		o
	ld a,h			;b587	7c		|
	rlc a			;b588	cb 07		. .
	rlc a			;b58a	cb 07		. .
	and 0e0h		;b58c	e6 e0		. .
	or l			;b58e	b5		.
	ld l,a			;b58f	6f		o
	ld a,h			;b590	7c		|
	and 007h		;b591	e6 07		. .
	ex af,af'		;b593	08		.
	ld a,h			;b594	7c		|
	rrc a			;b595	cb 0f		. .
	rrc a			;b597	cb 0f		. .
	rrc a			;b599	cb 0f		. .
	and 018h		;b59b	e6 18		. .
	or 040h			;b59d	f6 40		. @
	ld h,a			;b59f	67		g
	ex af,af'		;b5a0	08		.
	or h			;b5a1	b4		.
	ld h,a			;b5a2	67		g
	ret			;b5a3	c9		.
sub_b5a4h:
	srl h			;b5a4	cb 3c		. <
	srl h			;b5a6	cb 3c		. <
	srl h			;b5a8	cb 3c		. <
	srl h			;b5aa	cb 3c		. <
	rr l			;b5ac	cb 1d		. .
	srl h			;b5ae	cb 3c		. <
	rr l			;b5b0	cb 1d		. .
	srl h			;b5b2	cb 3c		. <
	rr l			;b5b4	cb 1d		. .
	ld a,h			;b5b6	7c		|
	add a,058h		;b5b7	c6 58		. X
	ld h,a			;b5b9	67		g
	ret			;b5ba	c9		.
sub_b5bbh:
	add a,l			;b5bb	85		.
	ld l,a			;b5bc	6f		o
	ret nc			;b5bd	d0		.
	inc h			;b5be	24		$
	ret			;b5bf	c9		.
sub_b5c0h:
	push hl			;b5c0	e5		.
	call sub_c03dh		;b5c1	cd 3d c0	. = .
	ld a,(de)		;b5c4	1a		.
	ld (lb5d8h+1),a		;b5c5	32 d9 b5	2 . .
	add a,01fh		;b5c8	c6 1f		. .
	ld (0b5e3h),a		;b5ca	32 e3 b5	2 . .
	inc de			;b5cd	13		.
	ld a,(de)		;b5ce	1a		.
	pop bc			;b5cf	c1		.
	push bc			;b5d0	c5		.
	cp b			;b5d1	b8		.
	jr c,lb5d6h		;b5d2	38 02		8 .
	inc b			;b5d4	04		.
	ld a,b			;b5d5	78		x
lb5d6h:
	inc de			;b5d6	13		.
	ld c,a			;b5d7	4f		O
lb5d8h:
	ld b,000h		;b5d8	06 00		. .
lb5dah:
	ld a,(de)		;b5da	1a		.
	ld (hl),a		;b5db	77		w
	inc de			;b5dc	13		.
	inc l			;b5dd	2c		,
	djnz lb5dah		;b5de	10 fa		. .
	dec l			;b5e0	2d		-
	ld a,l			;b5e1	7d		}
	sub 000h		;b5e2	d6 00		. .
	ld l,a			;b5e4	6f		o
	jp nc,lb5e9h		;b5e5	d2 e9 b5	. . .
	dec h			;b5e8	25		%
lb5e9h:
	dec c			;b5e9	0d		.
	jr nz,lb5d8h		;b5ea	20 ec		  .
	pop hl			;b5ec	e1		.
	ld a,h			;b5ed	7c		|
	cp 017h			;b5ee	fe 17		. .
	ret nz			;b5f0	c0		.
	ld a,008h		;b5f1	3e 08		> .
	add a,e			;b5f3	83		.
	ld e,a			;b5f4	5f		_
	ret nc			;b5f5	d0		.
	inc d			;b5f6	14		.
	ret			;b5f7	c9		.
sub_b5f8h:
	push hl			;b5f8	e5		.
	call sub_b57dh		;b5f9	cd 7d b5	. } .
	ld a,(de)		;b5fc	1a		.
	ld (0b606h),a		;b5fd	32 06 b6	2 . .
	inc de			;b600	13		.
	ld a,(de)		;b601	1a		.
	inc de			;b602	13		.
	ld c,a			;b603	4f		O
lb604h:
	push hl			;b604	e5		.
	ld b,000h		;b605	06 00		. .
lb607h:
	ld a,(de)		;b607	1a		.
	ld (hl),a		;b608	77		w
	inc de			;b609	13		.
	inc l			;b60a	2c		,
	djnz lb607h		;b60b	10 fa		. .
	pop hl			;b60d	e1		.
	call sub_b56eh		;b60e	cd 6e b5	. n .
	dec c			;b611	0d		.
	jr nz,lb604h		;b612	20 f0		  .
	pop hl			;b614	e1		.
	ret			;b615	c9		.
lb616h:
	nop			;b616	00		.
lb617h:
	nop			;b617	00		.
lb618h:
	nop			;b618	00		.
sub_b619h:
	ld de,lb616h		;b619	11 16 b6	. . .
sub_b61ch:
	push hl			;b61c	e5		.
	call sub_b5a4h		;b61d	cd a4 b5	. . .
	ld a,(de)		;b620	1a		.
	ld (lb632h+1),a		;b621	32 33 b6	2 3 .
	add a,01eh		;b624	c6 1e		. .
	cpl			;b626	2f		/
	ld (0b63bh),a		;b627	32 3b b6	2 ; .
	inc de			;b62a	13		.
	ld a,(de)		;b62b	1a		.
	ld c,a			;b62c	4f		O
	inc de			;b62d	13		.
	ld a,(de)		;b62e	1a		.
	ld (lb634h+1),a		;b62f	32 35 b6	2 5 .
lb632h:
	ld b,000h		;b632	06 00		. .
lb634h:
	ld (hl),000h		;b634	36 00		6 .
	inc l			;b636	2c		,
	djnz lb634h		;b637	10 fb		. .
	dec l			;b639	2d		-
	ld de,0ff00h		;b63a	11 00 ff	. . .
	add hl,de		;b63d	19		.
	dec c			;b63e	0d		.
	jr nz,lb632h		;b63f	20 f1		  .
	pop hl			;b641	e1		.
	ret			;b642	c9		.
sub_b643h:
	push hl			;b643	e5		.
	call sub_c051h		;b644	cd 51 c0	. Q .
	ld a,(de)		;b647	1a		.
	ld (lb654h+1),a		;b648	32 55 b6	2 U .
	add a,01fh		;b64b	c6 1f		. .
	ld (0b65fh),a		;b64d	32 5f b6	2 _ .
	inc de			;b650	13		.
	ld a,(de)		;b651	1a		.
	ld c,a			;b652	4f		O
	inc de			;b653	13		.
lb654h:
	ld b,000h		;b654	06 00		. .
lb656h:
	ld a,(de)		;b656	1a		.
	ld (hl),a		;b657	77		w
	inc l			;b658	2c		,
	inc de			;b659	13		.
	djnz lb656h		;b65a	10 fa		. .
	dec l			;b65c	2d		-
	ld a,l			;b65d	7d		}
	sub 000h		;b65e	d6 00		. .
	ld l,a			;b660	6f		o
	jp nc,lb665h		;b661	d2 65 b6	. e .
	dec h			;b664	25		%
lb665h:
	dec c			;b665	0d		.
	jr nz,lb654h		;b666	20 ec		  .
	pop hl			;b668	e1		.
	ret			;b669	c9		.
sub_b66ah:
	ld (sub_b678h+1),hl	;b66a	22 79 b6	" y .
	ld ix,l9ad0h		;b66d	dd 21 d0 9a	. ! . .
	ld b,00bh		;b671	06 0b		. .
lb673h:
	push bc			;b673	c5		.
	ld a,(ix+000h)		;b674	dd 7e 00	. ~ .
	add a,a			;b677	87		.
sub_b678h:
	call nz,sub_b678h	;b678	c4 78 b6	. x .
	ld de,00016h		;b67b	11 16 00	. . .
	add ix,de		;b67e	dd 19		. .
	pop bc			;b680	c1		.
	djnz lb673h		;b681	10 f0		. .
	ret			;b683	c9		.
sub_b684h:
	ld l,(ix+002h)		;b684	dd 6e 02	. n .
	ld h,(ix+004h)		;b687	dd 66 04	. f .
	call sub_c03dh		;b68a	cd 3d c0	. = .
	ld (ix+00ah),h		;b68d	dd 74 0a	. t .
	ld (ix+00bh),l		;b690	dd 75 0b	. u .
	ret			;b693	c9		.
sub_b694h:
	ld iy,lb6f4h		;b694	fd 21 f4 b6	. ! . .
	ld b,005h		;b698	06 05		. .
lb69ah:
	ld a,(iy+000h)		;b69a	fd 7e 00	. ~ .
	and a			;b69d	a7		.
	call nz,sub_b6a9h	;b69e	c4 a9 b6	. . .
	ld de,00007h		;b6a1	11 07 00	. . .
	add iy,de		;b6a4	fd 19		. .
	djnz lb69ah		;b6a6	10 f2		. .
	ret			;b6a8	c9		.
sub_b6a9h:
	ld l,(iy+005h)		;b6a9	fd 6e 05	. n .
	ld h,(iy+006h)		;b6ac	fd 66 06	. f .
	bit 7,(hl)		;b6af	cb 7e		. ~
	jr z,lb6b8h		;b6b1	28 05		( .
	ld (iy+000h),000h	;b6b3	fd 36 00 00	. 6 . .
	ret			;b6b7	c9		.
lb6b8h:
	exx			;b6b8	d9		.
	inc a			;b6b9	3c		<
	and 0feh		;b6ba	e6 fe		. .
	ld hl,0af6dh		;b6bc	21 6d af	! m .
	ld e,a			;b6bf	5f		_
	ld d,000h		;b6c0	16 00		. .
	add hl,de		;b6c2	19		.
	ld e,(hl)		;b6c3	5e		^
	inc hl			;b6c4	23		#
	ld d,(hl)		;b6c5	56		V
	ld l,(iy+001h)		;b6c6	fd 6e 01	. n .
	ld h,(iy+002h)		;b6c9	fd 66 02	. f .
	ld c,(iy+003h)		;b6cc	fd 4e 03	. N .
	ld b,(iy+004h)		;b6cf	fd 46 04	. F .
	ld a,007h		;b6d2	3e 07		> .
lb6d4h:
	ex af,af'		;b6d4	08		.
	ld a,(de)		;b6d5	1a		.
	ld (hl),a		;b6d6	77		w
	ld (bc),a		;b6d7	02		.
	inc l			;b6d8	2c		,
	inc c			;b6d9	0c		.
	inc de			;b6da	13		.
	ld a,(de)		;b6db	1a		.
	ld (hl),a		;b6dc	77		w
	ld (bc),a		;b6dd	02		.
	dec l			;b6de	2d		-
	inc h			;b6df	24		$
	inc de			;b6e0	13		.
	ld a,01fh		;b6e1	3e 1f		> .
	add a,c			;b6e3	81		.
	ld c,a			;b6e4	4f		O
	ex af,af'		;b6e5	08		.
	dec a			;b6e6	3d		=
	jr nz,lb6d4h		;b6e7	20 eb		  .
	exx			;b6e9	d9		.
	ld a,(iy+000h)		;b6ea	fd 7e 00	. ~ .
	inc a			;b6ed	3c		<
	and 00fh		;b6ee	e6 0f		. .
	ld (iy+000h),a		;b6f0	fd 77 00	. w .
	ret			;b6f3	c9		.
lb6f4h:
	nop			;b6f4	00		.
	nop			;b6f5	00		.
	nop			;b6f6	00		.
	nop			;b6f7	00		.
	nop			;b6f8	00		.
	nop			;b6f9	00		.
	nop			;b6fa	00		.
	nop			;b6fb	00		.
	nop			;b6fc	00		.
	nop			;b6fd	00		.
	nop			;b6fe	00		.
	nop			;b6ff	00		.
	nop			;b700	00		.
	nop			;b701	00		.
	nop			;b702	00		.
	nop			;b703	00		.
	nop			;b704	00		.
	nop			;b705	00		.
	nop			;b706	00		.
	nop			;b707	00		.
lb708h:
	nop			;b708	00		.
	nop			;b709	00		.
	nop			;b70a	00		.
	nop			;b70b	00		.
	nop			;b70c	00		.
	nop			;b70d	00		.
	nop			;b70e	00		.
	nop			;b70f	00		.
	nop			;b710	00		.
	nop			;b711	00		.
	nop			;b712	00		.
	nop			;b713	00		.
	nop			;b714	00		.
	nop			;b715	00		.
	nop			;b716	00		.
sub_b717h:
	ld b,a			;b717	47		G
	ld ix,l9c08h		;b718	dd 21 08 9c	. ! . .
lb71ch:
	push bc			;b71c	c5		.
	ld l,(ix+001h)		;b71d	dd 6e 01	. n .
	ld h,(ix+000h)		;b720	dd 66 00	. f .
	ld c,(ix+003h)		;b723	dd 4e 03	. N .
	ld b,(ix+002h)		;b726	dd 46 02	. F .
	call sub_9cf4h		;b729	cd f4 9c	. . .
	ld bc,00004h		;b72c	01 04 00	. . .
	add ix,bc		;b72f	dd 09		. .
	pop bc			;b731	c1		.
	djnz lb71ch		;b732	10 e8		. .
	xor a			;b734	af		.
	ld (l9c24h),a		;b735	32 24 9c	2 $ .
	ld hl,l9c08h		;b738	21 08 9c	! . .
	ld (lb2d8h+2),hl	;b73b	22 da b2	" . .
	ret			;b73e	c9		.
lb73fh:
	nop			;b73f	00		.
	ld a,0c9h		;b740	3e c9		> .
	ld (lb73fh),a		;b742	32 3f b7	2 ? .
	ld hl,(l9789h)		;b745	2a 89 97	* . .
	ld b,0b4h		;b748	06 b4		. .
lb74ah:
	ld a,(hl)		;b74a	7e		~
	and 090h		;b74b	e6 90		. .
	jr z,lb754h		;b74d	28 05		( .
	inc hl			;b74f	23		#
	djnz lb74ah		;b750	10 f8		. .
	jr lb764h		;b752	18 10		. .
lb754h:
	ld iy,(lb793h)		;b754	fd 2a 93 b7	. * . .
	push ix			;b758	dd e5		. .
	ld ix,lc0b8h		;b75a	dd 21 b8 c0	. ! . .
	call sub_c101h		;b75e	cd 01 c1	. . .
	di			;b761	f3		.
	pop ix			;b762	dd e1		. .
lb764h:
	ret			;b764	c9		.
sub_b765h:
	xor a			;b765	af		.
	ld (lb73fh),a		;b766	32 3f b7	2 ? .
	ld ix,laf6fh		;b769	dd 21 6f af	. ! o .
lb76dh:
	ld iy,(lb793h)		;b76d	fd 2a 93 b7	. * . .
	ei			;b771	fb		.
	halt			;b772	76		v
	ei			;b773	fb		.
	halt			;b774	76		v
	di			;b775	f3		.
	call sub_ad8fh		;b776	cd 8f ad	. . .
	ld de,laf3fh		;b779	11 3f af	. ? .
	ld l,(ix+000h)		;b77c	dd 6e 00	. n .
	ld h,(ix+001h)		;b77f	dd 66 01	. f .
	xor a			;b782	af		.
	sbc hl,de		;b783	ed 52		. R
	call z,lb73fh		;b785	cc 3f b7	. ? .
	inc ix			;b788	dd 23		. #
	inc ix			;b78a	dd 23		. #
	ld a,(ix+001h)		;b78c	dd 7e 01	. ~ .
	and a			;b78f	a7		.
	jr nz,lb76dh		;b790	20 db		  .
	ret			;b792	c9		.
lb793h:
	nop			;b793	00		.
	nop			;b794	00		.
sub_b795h:
	ret			;b795	c9		.
sub_b796h:
	push bc			;b796	c5		.
	call lb4ech		;b797	cd ec b4	. . .
	pop bc			;b79a	c1		.
	djnz sub_b796h		;b79b	10 f9		. .
	ret			;b79d	c9		.
	ex af,af'		;b79e	08		.
	rlca			;b79f	07		.
	ld b,h			;b7a0	44		D
	inc b			;b7a1	04		.
	ld bc,01e26h		;b7a2	01 26 1e	. & .
	add hl,de		;b7a5	19		.
lb7a6h:
	nop			;b7a6	00		.
	rrca			;b7a7	0f		.
	ld b,a			;b7a8	47		G
	ld b,000h		;b7a9	06 00		. .
	nop			;b7ab	00		.
lb7ach:
	nop			;b7ac	00		.
	nop			;b7ad	00		.
lb7aeh:
	nop			;b7ae	00		.
	nop			;b7af	00		.
	ld a,b			;b7b0	78		x
	rlca			;b7b1	07		.
	ld b,e			;b7b2	43		C
	ld (bc),a		;b7b3	02		.
	ld de,l6811h+1		;b7b4	11 12 68	. . h
	rrca			;b7b7	0f		.
	ld b,a			;b7b8	47		G
	ld b,000h		;b7b9	06 00		. .
	nop			;b7bb	00		.
lb7bch:
	djnz lb7beh		;b7bc	10 00		. .
lb7beh:
	nop			;b7be	00		.
	nop			;b7bf	00		.
	ret c			;b7c0	d8		.
	rlca			;b7c1	07		.
	ld b,h			;b7c2	44		D
	inc b			;b7c3	04		.
	ld (bc),a		;b7c4	02		.
	ld h,01eh		;b7c5	26 1e		& .
	add hl,de		;b7c7	19		.
lb7c8h:
	ret nc			;b7c8	d0		.
	rrca			;b7c9	0f		.
	ld b,a			;b7ca	47		G
	ld b,000h		;b7cb	06 00		. .
	nop			;b7cd	00		.
lb7ceh:
	nop			;b7ce	00		.
	nop			;b7cf	00		.
lb7d0h:
	nop			;b7d0	00		.
	nop			;b7d1	00		.
	ld b,007h		;b7d2	06 07		. .
lb7d4h:
	call sub_97d3h		;b7d4	cd d3 97	. . .
	djnz lb7d4h		;b7d7	10 fb		. .
	jp sub_97adh		;b7d9	c3 ad 97	. . .
lb7dch:
	ld d,000h		;b7dc	16 00		. .
	call sub_97d3h		;b7de	cd d3 97	. . .
	djnz lb7dch		;b7e1	10 f9		. .
	ret			;b7e3	c9		.
	ret			;b7e4	c9		.
lb7e5h:
	nop			;b7e5	00		.
lb7e6h:
	nop			;b7e6	00		.
lb7e7h:
	nop			;b7e7	00		.
lb7e8h:
	inc bc			;b7e8	03		.
lb7e9h:
	nop			;b7e9	00		.
lb7eah:
	nop			;b7ea	00		.
lb7ebh:
	nop			;b7eb	00		.
lb7ech:
	nop			;b7ec	00		.
lb7edh:
	nop			;b7ed	00		.
lb7eeh:
	nop			;b7ee	00		.
lb7efh:
	nop			;b7ef	00		.
lb7f0h:
	nop			;b7f0	00		.
	nop			;b7f1	00		.
lb7f2h:
	nop			;b7f2	00		.
	nop			;b7f3	00		.
lb7f4h:
	nop			;b7f4	00		.
	nop			;b7f5	00		.
lb7f6h:
	nop			;b7f6	00		.
lb7f7h:
	nop			;b7f7	00		.
sub_b7f8h:
	ld a,(0b842h)		;b7f8	3a 42 b8	: B .
	xor 088h		;b7fb	ee 88		. .
	ld (0b842h),a		;b7fd	32 42 b8	2 B .
	ld a,00ch		;b800	3e 0c		> .
	ld (l891dh),a		;b802	32 1d 89	2 . .
	ld de,l9ad0h		;b805	11 d0 9a	. . .
	ld hl,06000h		;b808	21 00 60	! . `
	ld a,00bh		;b80b	3e 0b		> .
lb80dh:
	ld bc,00016h		;b80d	01 16 00	. . .
	ldir			;b810	ed b0		. .
	dec a			;b812	3d		=
	jr nz,lb80dh		;b813	20 f8		  .
	ld (0baech),a		;b815	32 ec ba	2 . .
	ld (la85fh),a		;b818	32 5f a8	2 _ .
	ld (la66bh),a		;b81b	32 6b a6	2 k .
	ld (l8e71h),a		;b81e	32 71 8e	2 q .
	ld (l8ed9h),a		;b821	32 d9 8e	2 . .
	ld (lb972h),a		;b824	32 72 b9	2 r .
	inc a			;b827	3c		<
	ld (05cd9h),a		;b828	32 d9 5c	2 . \
	ld a,(lb7e5h)		;b82b	3a e5 b7	: . .
	cp 002h			;b82e	fe 02		. .
	jr nz,lb85ch		;b830	20 2a		  *
	ld a,001h		;b832	3e 01		> .
	ld (l9b3eh),a		;b834	32 3e 9b	2 > .
	ld a,038h		;b837	3e 38		> 8
	ld (l9b54h+2),a		;b839	32 56 9b	2 V .
	ld a,0b0h		;b83c	3e b0		> .
	ld (l9b40h),a		;b83e	32 40 9b	2 @ .
	ld a,048h		;b841	3e 48		> H
	ld (l9ad2h),a		;b843	32 d2 9a	2 . .
	cp 0c0h			;b846	fe c0		. .
	jr nz,lb85ch		;b848	20 12		  .
	ld a,(l9ae2h)		;b84a	3a e2 9a	: . .
	or 080h			;b84d	f6 80		. .
	ld (l9ae2h),a		;b84f	32 e2 9a	2 . .
	ld a,0ffh		;b852	3e ff		> .
	ld (l9b68h),a		;b854	32 68 9b	2 h .
	ld a,083h		;b857	3e 83		> .
	ld (l9b52h),a		;b859	32 52 9b	2 R .
lb85ch:
	ld hl,l8cc0h		;b85c	21 c0 8c	! . .
	ld (l9ae4h),hl		;b85f	22 e4 9a	" . .
	ld l,008h		;b862	2e 08		. .
	ld a,(lb7eah)		;b864	3a ea b7	: . .
	add a,002h		;b867	c6 02		. .
	cp 004h			;b869	fe 04		. .
	jr c,lb86fh		;b86b	38 02		8 .
	ld a,004h		;b86d	3e 04		> .
lb86fh:
	ld a,003h		;b86f	3e 03		> .
	ld h,a			;b871	67		g
	ld (l9ad6h),hl		;b872	22 d6 9a	" . .
	ld a,00eh		;b875	3e 0e		> .
	ld (0b971h),a		;b877	32 71 b9	2 q .
	ld (lb8d6h),a		;b87a	32 d6 b8	2 . .
	ld a,083h		;b87d	3e 83		> .
	ld (l9b68h),a		;b87f	32 68 9b	2 h .
	xor a			;b882	af		.
	ld (l9c24h),a		;b883	32 24 9c	2 $ .
	ld hl,l9c08h		;b886	21 08 9c	! . .
	ld (lb2d8h+2),hl	;b889	22 da b2	" . .
	ld hl,09e6ah		;b88c	21 6a 9e	! j .
	ld a,(lb7ebh)		;b88f	3a eb b7	: . .
	cp 006h			;b892	fe 06		. .
	jr c,lb899h		;b894	38 03		8 .
	ld hl,09e8ah		;b896	21 8a 9e	! . .
lb899h:
	ld de,l9e4ah		;b899	11 4a 9e	. J .
	ld bc,00010h		;b89c	01 10 00	. . .
	ldir			;b89f	ed b0		. .
	ld hl,la270h		;b8a1	21 70 a2	! p .
	ld b,00ch		;b8a4	06 0c		. .
	call l8ed9h+1		;b8a6	cd da 8e	. . .
	ld hl,lc0b8h		;b8a9	21 b8 c0	! . .
	ld b,023h		;b8ac	06 23		. #
	call l8ed9h+1		;b8ae	cd da 8e	. . .
	ld hl,lb6f4h		;b8b1	21 f4 b6	! . .
	ld b,023h		;b8b4	06 23		. #
	jp l8ed9h+1		;b8b6	c3 da 8e	. . .
sub_b8b9h:
	ld b,0b4h		;b8b9	06 b4		. .
lb8bbh:
	ld a,(hl)		;b8bb	7e		~
	cp 0c0h			;b8bc	fe c0		. .
	jr z,lb8d2h		;b8be	28 12		( .
	bit 5,a			;b8c0	cb 6f		. o
	jr nz,lb8d2h		;b8c2	20 0e		  .
	res 7,(hl)		;b8c4	cb be		. .
	res 6,(hl)		;b8c6	cb b6		. .
	set 4,(hl)		;b8c8	cb e6		. .
	and 00fh		;b8ca	e6 0f		. .
	cp 006h			;b8cc	fe 06		. .
	jr c,lb8d2h		;b8ce	38 02		8 .
	res 4,(hl)		;b8d0	cb a6		. .
lb8d2h:
	inc hl			;b8d2	23		#
	djnz lb8bbh		;b8d3	10 e6		. .
	ret			;b8d5	c9		.
lb8d6h:
	ld c,03ah		;b8d6	0e 3a		. :
	ld (hl),c		;b8d8	71		q
	cp c			;b8d9	b9		.
	ld b,a			;b8da	47		G
	ld a,(lb8d6h)		;b8db	3a d6 b8	: . .
	ld (0b971h),a		;b8de	32 71 b9	2 q .
	ld a,b			;b8e1	78		x
	ld (lb8d6h),a		;b8e2	32 d6 b8	2 . .
	ret			;b8e5	c9		.
sub_b8e6h:
	ld de,0f060h		;b8e6	11 60 f0	. ` .
	ld a,(0b971h)		;b8e9	3a 71 b9	: q .
	and 07fh		;b8ec	e6 7f		. .
	ld b,a			;b8ee	47		G
	ld a,(ix+00ch)		;b8ef	dd 7e 0c	. ~ .
	sub b			;b8f2	90		.
	cp 009h			;b8f3	fe 09		. .
	jr nc,lb906h		;b8f5	30 0f		0 .
	ld a,(ix+00ch)		;b8f7	dd 7e 0c	. ~ .
	sub 00bh		;b8fa	d6 0b		. .
	ld b,a			;b8fc	47		G
	ld a,(0b971h)		;b8fd	3a 71 b9	: q .
	and 080h		;b900	e6 80		. .
	or b			;b902	b0		.
	ld (0b971h),a		;b903	32 71 b9	2 q .
lb906h:
	ld a,(ix+002h)		;b906	dd 7e 02	. ~ .
	add a,b			;b909	80		.
	ld c,a			;b90a	4f		O
	rra			;b90b	1f		.
	rra			;b90c	1f		.
	rra			;b90d	1f		.
	and 01fh		;b90e	e6 1f		. .
	add a,e			;b910	83		.
	ld e,a			;b911	5f		_
	ld a,c			;b912	79		y
	and 007h		;b913	e6 07		. .
	ld hl,lb969h		;b915	21 69 b9	! i .
	call sub_b5bbh		;b918	cd bb b5	. . .
	ld a,(de)		;b91b	1a		.
	and (hl)		;b91c	a6		.
	ld (de),a		;b91d	12		.
	ld a,(ix+002h)		;b91e	dd 7e 02	. ~ .
	ld e,a			;b921	5f		_
	ld a,(ix+00ch)		;b922	dd 7e 0c	. ~ .
	add a,e			;b925	83		.
	sub b			;b926	90		.
	dec a			;b927	3d		=
	ld c,a			;b928	4f		O
	rra			;b929	1f		.
	rra			;b92a	1f		.
	rra			;b92b	1f		.
	and 01fh		;b92c	e6 1f		. .
	add a,060h		;b92e	c6 60		. `
	ld e,a			;b930	5f		_
	ld a,c			;b931	79		y
	and 007h		;b932	e6 07		. .
	ld hl,lb969h		;b934	21 69 b9	! i .
	call sub_b5bbh		;b937	cd bb b5	. . .
	ld a,(de)		;b93a	1a		.
	and (hl)		;b93b	a6		.
	ld (de),a		;b93c	12		.
	ld a,(0b971h)		;b93d	3a 71 b9	: q .
	bit 7,a			;b940	cb 7f		. .
	res 7,a			;b942	cb bf		. .
	jr z,lb951h		;b944	28 0b		( .
	dec a			;b946	3d		=
	cp 009h			;b947	fe 09		. .
	jr z,lb95bh		;b949	28 10		( .
	or 080h			;b94b	f6 80		. .
	ld (0b971h),a		;b94d	32 71 b9	2 q .
	ret			;b950	c9		.
lb951h:
	inc a			;b951	3c		<
	ld b,a			;b952	47		G
	ld a,(ix+00ch)		;b953	dd 7e 0c	. ~ .
	sub b			;b956	90		.
	cp 00ah			;b957	fe 0a		. .
	jr nz,lb964h		;b959	20 09		  .
lb95bh:
	ld a,(0b971h)		;b95b	3a 71 b9	: q .
	xor 080h		;b95e	ee 80		. .
	ld (0b971h),a		;b960	32 71 b9	2 q .
	ret			;b963	c9		.
lb964h:
	ld a,b			;b964	78		x
	ld (0b971h),a		;b965	32 71 b9	2 q .
	ret			;b968	c9		.
lb969h:
	ld a,a			;b969	7f		.
	cp a			;b96a	bf		.
	rst 18h			;b96b	df		.
	rst 28h			;b96c	ef		.
	rst 30h			;b96d	f7		.
	ei			;b96e	fb		.
	defb 0fdh,0feh,00eh ;illegal sequence	;b96f	fd fe 0e	. . .
lb972h:
	nop			;b972	00		.
	ld bc,l733ah		;b973	01 3a 73	. : s
	cp c			;b976	b9		.
	and a			;b977	a7		.
	ret nz			;b978	c0		.
	ld de,lb989h		;b979	11 89 b9	. . .
	ld b,002h		;b97c	06 02		. .
	call sub_b796h		;b97e	cd 96 b7	. . .
	ld d,000h		;b981	16 00		. .
	call sub_97d3h		;b983	cd d3 97	. . .
	jp sub_97adh		;b986	c3 ad 97	. . .
lb989h:
	jr c,$+57		;b989	38 37		8 7
	ld b,a			;b98b	47		G
	inc de			;b98c	13		.
	inc d			;b98d	14		.
	ld (de),a		;b98e	12		.
	rla			;b98f	17		.
	rla			;b990	17		.
	jr $+14			;b991	18 0c		. .
	inc d			;b993	14		.
	ld h,00ch		;b994	26 0c		& .
	jr $+32			;b996	18 1e		. .
	dec d			;b998	15		.
	dec c			;b999	0d		.
	rla			;b99a	17		.
	dec e			;b99b	1d		.
	ld h,01bh		;b99c	26 1b		& .
	ld e,017h		;b99e	1e 17		. .
	ld d,b			;b9a0	50		P
	ld b,a			;b9a1	47		G
	ld b,a			;b9a2	47		G
	dec c			;b9a3	0d		.
	ld a,(bc)		;b9a4	0a		.
	ld h,022h		;b9a5	26 22		& "
	jr lb9c7h		;b9a7	18 1e		. .
	dec e			;b9a9	1d		.
	ld de,00c26h		;b9aa	11 26 0c	. & .
	dec d			;b9ad	15		.
	ld e,00bh		;b9ae	1e 0b		. .
	inc h			;b9b0	24		$
lb9b1h:
	ld (lb793h),iy		;b9b1	fd 22 93 b7	. " . .
	ld hl,01510h		;b9b5	21 10 15	! . .
	call sub_c03dh		;b9b8	cd 3d c0	. = .
	ld (lb7a6h),hl		;b9bb	22 a6 b7	" . .
	ld hl,015c0h		;b9be	21 c0 15	! . .
	call sub_c03dh		;b9c1	cd 3d c0	. = .
	ld (lb7c8h),hl		;b9c4	22 c8 b7	" . .
lb9c7h:
	ld hl,01568h		;b9c7	21 68 15	! h .
	call sub_c03dh		;b9ca	cd 3d c0	. = .
	ld (0b7b6h),hl		;b9cd	22 b6 b7	" . .
	ld de,06000h		;b9d0	11 00 60	. . `
	ld hl,l9ad0h		;b9d3	21 d0 9a	! . .
	ld a,00bh		;b9d6	3e 0b		> .
lb9d8h:
	ld bc,00016h		;b9d8	01 16 00	. . .
	ldir			;b9db	ed b0		. .
	dec a			;b9dd	3d		=
	jr nz,lb9d8h		;b9de	20 f8		  .
	ld hl,05cd8h		;b9e0	21 d8 5c	! . \
	ld b,001h		;b9e3	06 01		. .
	call l8ed9h+1		;b9e5	cd da 8e	. . .
lb9e8h:
	ld a,(lb7a6h)		;b9e8	3a a6 b7	: . .
	and 01fh		;b9eb	e6 1f		. .
	cp 002h			;b9ed	fe 02		. .
	call nz,sub_be30h	;b9ef	c4 30 be	. 0 .
	call 093f8h		;b9f2	cd f8 93	. . .
	ld hl,00000h		;b9f5	21 00 00	! . .
	ld (lb7ech),hl		;b9f8	22 ec b7	" . .
	ld (lb7edh),hl		;b9fb	22 ed b7	" . .
	ld (0b7aah),hl		;b9fe	22 aa b7	" . .
	ld (lb7ach),hl		;ba01	22 ac b7	" . .
	ld (lb7aeh),hl		;ba04	22 ae b7	" . .
	ld (0b7cch),hl		;ba07	22 cc b7	" . .
	ld (lb7ceh),hl		;ba0a	22 ce b7	" . .
	ld (lb7d0h),hl		;ba0d	22 d0 b7	" . .
	ld a,003h		;ba10	3e 03		> .
	ld (lb7e8h),a		;ba12	32 e8 b7	2 . .
	ld a,0c0h		;ba15	3e c0		> .
	ld (0b842h),a		;ba17	32 42 b8	2 B .
	xor a			;ba1a	af		.
	ld (lb7eah),a		;ba1b	32 ea b7	2 . .
	ld (lb7ebh),a		;ba1e	32 eb b7	2 . .
	ld (lb7e6h),a		;ba21	32 e6 b7	2 . .
	call sub_be54h		;ba24	cd 54 be	. T .
	ld de,06100h		;ba27	11 00 61	. . a
	ld hl,(l9789h)		;ba2a	2a 89 97	* . .
	ld bc,000b4h		;ba2d	01 b4 00	. . .
	ldir			;ba30	ed b0		. .
	ld de,lb7f0h		;ba32	11 f0 b7	. . .
	ld hl,lb7e8h		;ba35	21 e8 b7	! . .
	ld bc,00007h		;ba38	01 07 00	. . .
	ldir			;ba3b	ed b0		. .
	ld a,(lb7e5h)		;ba3d	3a e5 b7	: . .
	and a			;ba40	a7		.
	jr nz,lba46h		;ba41	20 03		  .
	ld (lb7f0h),a		;ba43	32 f0 b7	2 . .
lba46h:
	call sub_97adh		;ba46	cd ad 97	. . .
	call sub_97bch		;ba49	cd bc 97	. . .
lba4ch:
	call sub_9776h		;ba4c	cd 76 97	. v .
	call sub_be8bh		;ba4f	cd 8b be	. . .
	call sub_b7f8h		;ba52	cd f8 b7	. . .
	call sub_97adh		;ba55	cd ad 97	. . .
	call 0b974h		;ba58	cd 74 b9	. t .
	call sub_bdcfh		;ba5b	cd cf bd	. . .
	call sub_bdf6h		;ba5e	cd f6 bd	. . .
	call sub_b795h		;ba61	cd 95 b7	. . .
	call sub_8f60h		;ba64	cd 60 8f	. ` .
	ld b,004h		;ba67	06 04		. .
	call lb7dch		;ba69	cd dc b7	. . .
	call sub_b765h		;ba6c	cd 65 b7	. e .
	ld hl,08158h		;ba6f	21 58 81	! X .
	ld bc,00a28h		;ba72	01 28 0a	. ( .
	call sub_9cf4h		;ba75	cd f4 9c	. . .
	ld hl,0d90bh		;ba78	21 0b d9	! . .
	ld de,05a0bh		;ba7b	11 0b 5a	. . Z
	ld bc,0008bh		;ba7e	01 8b 00	. . .
	ldir			;ba81	ed b0		. .
lba83h:
	ld a,(l8d49h)		;ba83	3a 49 8d	: I .
	cp 099h			;ba86	fe 99		. .
	call z,sub_8e72h	;ba88	cc 72 8e	. r .
	xor a			;ba8b	af		.
	ld (05cdch),a		;ba8c	32 dc 5c	2 . \
	call sub_a161h		;ba8f	cd 61 a1	. a .
	ld hl,(l8d46h)		;ba92	2a 46 8d	* F .
	inc hl			;ba95	23		#
	ld (l8d46h),hl		;ba96	22 46 8d	" F .
	call 09eaah		;ba99	cd aa 9e	. . .
	call sub_8eb4h		;ba9c	cd b4 8e	. . .
	ld ix,l9b54h		;ba9f	dd 21 54 9b	. ! T .
	call sub_9f64h		;baa3	cd 64 9f	. d .
	ld a,(lb7e5h)		;baa6	3a e5 b7	: . .
	cp 002h			;baa9	fe 02		. .
	jr nz,lbadch		;baab	20 2f		  /
	call sub_a66ch		;baad	cd 6c a6	. l .
	ld a,(l8ed9h)		;bab0	3a d9 8e	: . .
	push af			;bab3	f5		.
	ld a,(lb7f7h)		;bab4	3a f7 b7	: . .
	call sub_a19eh		;bab7	cd 9e a1	. . .
	ld a,(l8ed9h)		;baba	3a d9 8e	: . .
	ld (lb972h),a		;babd	32 72 b9	2 r .
	ld ix,l9b3eh		;bac0	dd 21 3e 9b	. ! > .
	call sub_9f64h		;bac4	cd 64 9f	. d .
	pop af			;bac7	f1		.
	ld (l8ed9h),a		;bac8	32 d9 8e	2 . .
	ld ix,l9b54h		;bacb	dd 21 54 9b	. ! T .
	call sub_acceh		;bacf	cd ce ac	. . .
	ld ix,l9b3eh		;bad2	dd 21 3e 9b	. ! > .
	call sub_acadh		;bad6	cd ad ac	. . .
	call sub_a66ch		;bad9	cd 6c a6	. l .
lbadch:
	ld hl,l9f54h		;badc	21 54 9f	! T .
	call sub_b66ah		;badf	cd 6a b6	. j .
	ld hl,sub_b684h		;bae2	21 84 b6	! . .
	call sub_b66ah		;bae5	cd 6a b6	. j .
	call sub_b694h		;bae8	cd 94 b6	. . .
	jr lbaf4h		;baeb	18 07		. .
	ld a,(l9bach)		;baed	3a ac 9b	: . .
	and a			;baf0	a7		.
	jp nz,lbb6ah		;baf1	c2 6a bb	. j .
lbaf4h:
	ld a,(05cd9h)		;baf4	3a d9 5c	: . \
	and a			;baf7	a7		.
	jp z,lbc10h		;baf8	ca 10 bc	. . .
	ld a,(lb7e9h)		;bafb	3a e9 b7	: . .
	and a			;bafe	a7		.
	jp z,lbc10h		;baff	ca 10 bc	. . .
	call sub_97deh		;bb02	cd de 97	. . .
	ld hl,sub_9910h		;bb05	21 10 99	! . .
	call sub_b66ah		;bb08	cd 6a b6	. j .
	call sub_c077h		;bb0b	cd 77 c0	. w .
	jr nz,lbb39h		;bb0e	20 29		  )
	ld a,(0d000h)		;bb10	3a 00 d0	: . .
	cp 004h			;bb13	fe 04		. .
	jr z,lbb22h		;bb15	28 0b		( .
	jr c,lbb36h		;bb17	38 1d		8 .
	ld a,(05cd8h)		;bb19	3a d8 5c	: . \
	cp 023h			;bb1c	fe 23		. #
	jr nc,lbb39h		;bb1e	30 19		0 .
	jr lbb36h		;bb20	18 14		. .
lbb22h:
	ld a,(l9b60h)		;bb22	3a 60 9b	: ` .
	cp 01ch			;bb25	fe 1c		. .
	jr nz,lbb2fh		;bb27	20 06		  .
	ld a,(l9b80h)		;bb29	3a 80 9b	: . .
	and a			;bb2c	a7		.
	jr nz,lbb36h		;bb2d	20 07		  .
lbb2fh:
	ld a,(05cdch)		;bb2f	3a dc 5c	: . \
	cp 003h			;bb32	fe 03		. .
	jr c,lbb39h		;bb34	38 03		8 .
lbb36h:
	ei			;bb36	fb		.
	halt			;bb37	76		v
	di			;bb38	f3		.
lbb39h:
	ld ix,l9b54h		;bb39	dd 21 54 9b	. ! T .
	call sub_b8e6h		;bb3d	cd e6 b8	. . .
	ld a,(lb7e5h)		;bb40	3a e5 b7	: . .
	cp 002h			;bb43	fe 02		. .
	jr nz,lbb54h		;bb45	20 0d		  .
	call lb8d6h+1		;bb47	cd d7 b8	. . .
	ld ix,l9b3eh		;bb4a	dd 21 3e 9b	. ! > .
	call sub_b8e6h		;bb4e	cd e6 b8	. . .
	call lb8d6h+1		;bb51	cd d7 b8	. . .
lbb54h:
	ld hl,sub_9c25h		;bb54	21 25 9c	! % .
	call sub_b66ah		;bb57	cd 6a b6	. j .
	ld a,(l9c24h)		;bb5a	3a 24 9c	: $ .
	and a			;bb5d	a7		.
	call nz,sub_b717h	;bb5e	c4 17 b7	. . .
	call sub_987ah		;bb61	cd 7a 98	. z .
	call sub_978bh		;bb64	cd 8b 97	. . .
	jp lba83h		;bb67	c3 83 ba	. . .
lbb6ah:
	ld b,00bh		;bb6a	06 0b		. .
	ld de,00016h		;bb6c	11 16 00	. . .
	ld ix,l9ad0h		;bb6f	dd 21 d0 9a	. ! . .
lbb73h:
	ld a,(ix+000h)		;bb73	dd 7e 00	. ~ .
	and a			;bb76	a7		.
	jr z,lbb7dh		;bb77	28 04		( .
	set 7,(ix+000h)		;bb79	dd cb 00 fe	. . . .
lbb7dh:
	add ix,de		;bb7d	dd 19		. .
	djnz lbb73h		;bb7f	10 f2		. .
	ld a,001h		;bb81	3e 01		> .
	ld hl,00000h		;bb83	21 00 00	! . .
	ld (hl),a		;bb86	77		w
	ld a,006h		;bb87	3e 06		> .
	ld (l9bach),a		;bb89	32 ac 9b	2 . .
	ld a,005h		;bb8c	3e 05		> .
	ld (lc0b8h),a		;bb8e	32 b8 c0	2 . .
	xor a			;bb91	af		.
	ld (l8d46h),a		;bb92	32 46 8d	2 F .
	jr lbbaah		;bb95	18 13		. .
lbb97h:
	ld a,(l8d46h)		;bb97	3a 46 8d	: F .
	inc a			;bb9a	3c		<
	ld (l8d46h),a		;bb9b	32 46 8d	2 F .
	call sub_8eb4h		;bb9e	cd b4 8e	. . .
	ld hl,l9f54h		;bba1	21 54 9f	! T .
	call sub_b66ah		;bba4	cd 6a b6	. j .
	call sub_b694h		;bba7	cd 94 b6	. . .
lbbaah:
	ld hl,sub_b684h		;bbaa	21 84 b6	! . .
	call sub_b66ah		;bbad	cd 6a b6	. j .
	call sub_97deh		;bbb0	cd de 97	. . .
	ld hl,sub_9910h		;bbb3	21 10 99	! . .
	call sub_b66ah		;bbb6	cd 6a b6	. j .
	ld iy,(lb793h)		;bbb9	fd 2a 93 b7	. * . .
	ei			;bbbd	fb		.
	halt			;bbbe	76		v
	di			;bbbf	f3		.
	call sub_c077h		;bbc0	cd 77 c0	. w .
	ld hl,sub_9c25h		;bbc3	21 25 9c	! % .
	call sub_b66ah		;bbc6	cd 6a b6	. j .
	ld a,(l9c24h)		;bbc9	3a 24 9c	: $ .
	and a			;bbcc	a7		.
	call nz,sub_b717h	;bbcd	c4 17 b7	. . .
	call l989ah		;bbd0	cd 9a 98	. . .
	call sub_978bh		;bbd3	cd 8b 97	. . .
	ld a,(l9bach)		;bbd6	3a ac 9b	: . .
	and a			;bbd9	a7		.
	jp z,lbbfbh		;bbda	ca fb bb	. . .
	jp lbb97h		;bbdd	c3 97 bb	. . .
sub_bbe0h:
	ld a,(lb7ebh)		;bbe0	3a eb b7	: . .
	inc a			;bbe3	3c		<
	ld (lb7ebh),a		;bbe4	32 eb b7	2 . .
	ld a,(lb7eah)		;bbe7	3a ea b7	: . .
	inc a			;bbea	3c		<
	cp 00fh			;bbeb	fe 0f		. .
	jr z,lbbf4h		;bbed	28 05		( .
	ld (lb7eah),a		;bbef	32 ea b7	2 . .
	jr lbbf8h		;bbf2	18 04		. .
lbbf4h:
	xor a			;bbf4	af		.
	ld (lb7eah),a		;bbf5	32 ea b7	2 . .
lbbf8h:
	jp sub_be54h		;bbf8	c3 54 be	. T .
lbbfbh:
	call sub_af81h		;bbfb	cd 81 af	. . .
lbbfeh:
	ld a,(lb7e9h)		;bbfe	3a e9 b7	: . .
	and a			;bc01	a7		.
	call z,sub_c077h	;bc02	cc 77 c0	. w .
	call sub_bbe0h		;bc05	cd e0 bb	. . .
	ld b,002h		;bc08	06 02		. .
	call lb7dch		;bc0a	cd dc b7	. . .
	jp lba4ch		;bc0d	c3 4c ba	. L .
lbc10h:
	ld a,(l8e71h)		;bc10	3a 71 8e	: q .
	and a			;bc13	a7		.
	jr z,lbc30h		;bc14	28 1a		( .
	ld ix,(l8e70h)		;bc16	dd 2a 70 8e	. * p .
	ld a,(ix+002h)		;bc1a	dd 7e 02	. ~ .
	sub 005h		;bc1d	d6 05		. .
	ld l,a			;bc1f	6f		o
	ld a,(ix+004h)		;bc20	dd 7e 04	. ~ .
	sub 005h		;bc23	d6 05		. .
	ld h,a			;bc25	67		g
	ld bc,00417h		;bc26	01 17 04	. . .
	call sub_9cf4h		;bc29	cd f4 9c	. . .
	xor a			;bc2c	af		.
	ld (l8e71h),a		;bc2d	32 71 8e	2 q .
lbc30h:
	ld ix,l9ad0h		;bc30	dd 21 d0 9a	. ! . .
	ld b,00bh		;bc34	06 0b		. .
	ld de,00016h		;bc36	11 16 00	. . .
	ld a,(l9bach)		;bc39	3a ac 9b	: . .
	push af			;bc3c	f5		.
lbc3dh:
	ld a,(ix+000h)		;bc3d	dd 7e 00	. ~ .
	and a			;bc40	a7		.
	jr z,lbc47h		;bc41	28 04		( .
	set 7,(ix+000h)		;bc43	dd cb 00 fe	. . . .
lbc47h:
	add ix,de		;bc47	dd 19		. .
	djnz lbc3dh		;bc49	10 f2		. .
	pop af			;bc4b	f1		.
	ld (l9bach),a		;bc4c	32 ac 9b	2 . .
	ld hl,sub_9910h		;bc4f	21 10 99	! . .
	call sub_b66ah		;bc52	cd 6a b6	. j .
	ld hl,sub_9c25h		;bc55	21 25 9c	! % .
	call sub_b66ah		;bc58	cd 6a b6	. j .
	ld a,(l9c24h)		;bc5b	3a 24 9c	: $ .
	and a			;bc5e	a7		.
	call nz,sub_b717h	;bc5f	c4 17 b7	. . .
	ld a,(lb7e9h)		;bc62	3a e9 b7	: . .
	and a			;bc65	a7		.
	jp z,lbbfeh		;bc66	ca fe bb	. . .
	ld a,008h		;bc69	3e 08		> .
	ld (lc0b8h),a		;bc6b	32 b8 c0	2 . .
	ld a,03dh		;bc6e	3e 3d		> =
	ld (lc0b9h),a		;bc70	32 b9 c0	2 . .
	xor a			;bc73	af		.
	ld (la899h),a		;bc74	32 99 a8	2 . .
	ld ix,l9ad0h		;bc77	dd 21 d0 9a	. ! . .
	ld b,00ah		;bc7b	06 0a		. .
	ld a,(l9b54h+2)		;bc7d	3a 56 9b	: V .
	ld c,a			;bc80	4f		O
	ld a,(l9b40h)		;bc81	3a 40 9b	: @ .
	sub c			;bc84	91		.
	ld (0bce7h),a		;bc85	32 e7 bc	2 . .
	ld a,(l9b60h)		;bc88	3a 60 9b	: ` .
	srl a			;bc8b	cb 3f		. ?
	add a,c			;bc8d	81		.
	sub 00ch		;bc8e	d6 0c		. .
	ld c,a			;bc90	4f		O
	ld de,00016h		;bc91	11 16 00	. . .
	ld l,01bh		;bc94	2e 1b		. .
lbc96h:
	ld (ix+014h),018h	;bc96	dd 36 14 18	. 6 . .
	ld (ix+015h),018h	;bc9a	dd 36 15 18	. 6 . .
	ld (ix+00ch),008h	;bc9e	dd 36 0c 08	. 6 . .
	ld (ix+00dh),007h	;bca2	dd 36 0d 07	. 6 . .
	ld (ix+008h),002h	;bca6	dd 36 08 02	. 6 . .
	ld (ix+009h),00bh	;bcaa	dd 36 09 0b	. 6 . .
	ld (ix+011h),d		;bcae	dd 72 11	. r .
	ld (ix+001h),d		;bcb1	dd 72 01	. r .
	ld (ix+000h),007h	;bcb4	dd 36 00 07	. 6 . .
	ld (ix+002h),c		;bcb8	dd 71 02	. q .
	ld (ix+004h),0aeh	;bcbb	dd 36 04 ae	. 6 . .
	ld (ix+006h),l		;bcbf	dd 75 06	. u .
	ld (ix+007h),002h	;bcc2	dd 36 07 02	. 6 . .
	ld a,l			;bcc6	7d		}
	add a,005h		;bcc7	c6 05		. .
	and 03fh		;bcc9	e6 3f		. ?
	ld l,a			;bccb	6f		o
	add ix,de		;bccc	dd 19		. .
	inc c			;bcce	0c		.
	inc c			;bccf	0c		.
	inc c			;bcd0	0c		.
	djnz lbc96h		;bcd1	10 c3		. .
	ld a,(lb7e5h)		;bcd3	3a e5 b7	: . .
	cp 002h			;bcd6	fe 02		. .
	jr nz,lbcf1h		;bcd8	20 17		  .
	ld ix,l9ae6h		;bcda	dd 21 e6 9a	. ! . .
	ld de,00016h		;bcde	11 16 00	. . .
	ld b,005h		;bce1	06 05		. .
lbce3h:
	ld a,(ix+002h)		;bce3	dd 7e 02	. ~ .
	add a,000h		;bce6	c6 00		. .
	ld (ix+002h),a		;bce8	dd 77 02	. w .
	add ix,de		;bceb	dd 19		. .
	add ix,de		;bced	dd 19		. .
	djnz lbce3h		;bcef	10 f2		. .
lbcf1h:
	call sub_8eb4h		;bcf1	cd b4 8e	. . .
	ld hl,l9f54h		;bcf4	21 54 9f	! T .
	call sub_b66ah		;bcf7	cd 6a b6	. j .
	call sub_b694h		;bcfa	cd 94 b6	. . .
	ld hl,sub_b684h		;bcfd	21 84 b6	! . .
	call sub_b66ah		;bd00	cd 6a b6	. j .
	call sub_97deh		;bd03	cd de 97	. . .
	ld hl,sub_9910h		;bd06	21 10 99	! . .
	ld a,(l9ae6h)		;bd09	3a e6 9a	: . .
	rla			;bd0c	17		.
	call nc,sub_b66ah	;bd0d	d4 6a b6	. j .
	call sub_c077h		;bd10	cd 77 c0	. w .
	ld hl,sub_9c25h		;bd13	21 25 9c	! % .
	call sub_b66ah		;bd16	cd 6a b6	. j .
	ld a,(l9c24h)		;bd19	3a 24 9c	: $ .
	and a			;bd1c	a7		.
	call nz,sub_b717h	;bd1d	c4 17 b7	. . .
	call l989ah		;bd20	cd 9a 98	. . .
	call sub_978bh		;bd23	cd 8b 97	. . .
	ld a,(l9ae6h)		;bd26	3a e6 9a	: . .
	and a			;bd29	a7		.
	jp nz,lbcf1h		;bd2a	c2 f1 bc	. . .
	ld b,003h		;bd2d	06 03		. .
	call lb7dch		;bd2f	cd dc b7	. . .
	ld a,(lb7e8h)		;bd32	3a e8 b7	: . .
	dec a			;bd35	3d		=
	ld (lb7e8h),a		;bd36	32 e8 b7	2 . .
	jr z,lbd45h		;bd39	28 0a		( .
	ld a,(lb7e5h)		;bd3b	3a e5 b7	: . .
	dec a			;bd3e	3d		=
	call z,sub_be0ch	;bd3f	cc 0c be	. . .
	jp lba4ch		;bd42	c3 4c ba	. L .
lbd45h:
	ld b,002h		;bd45	06 02		. .
	call lb7d4h		;bd47	cd d4 b7	. . .
	call sub_97adh		;bd4a	cd ad 97	. . .
	call sub_97bch		;bd4d	cd bc 97	. . .
	ld a,(lb7e6h)		;bd50	3a e6 b7	: . .
	inc a			;bd53	3c		<
	ld (lbdb2h),a		;bd54	32 b2 bd	2 . .
	ld de,lbd99h		;bd57	11 99 bd	. . .
	ld b,002h		;bd5a	06 02		. .
	call sub_b796h		;bd5c	cd 96 b7	. . .
	ld a,(lb7e5h)		;bd5f	3a e5 b7	: . .
	cp 002h			;bd62	fe 02		. .
	call z,sub_bdb3h	;bd64	cc b3 bd	. . .
	ld b,00ch		;bd67	06 0c		. .
	call lb7d4h		;bd69	cd d4 b7	. . .
	call sub_be6eh		;bd6c	cd 6e be	. n .
	call sub_910ch		;bd6f	cd 0c 91	. . .
	ld a,(lb7e5h)		;bd72	3a e5 b7	: . .
	cp 002h			;bd75	fe 02		. .
	jr nz,lbd88h		;bd77	20 0f		  .
	call sub_be30h		;bd79	cd 30 be	. 0 .
	call sub_be6eh		;bd7c	cd 6e be	. n .
	call sub_910ch		;bd7f	cd 0c 91	. . .
	call sub_be30h		;bd82	cd 30 be	. 0 .
	jp lb9e8h		;bd85	c3 e8 b9	. . .
lbd88h:
	dec a			;bd88	3d		=
	jp nz,lb9e8h		;bd89	c2 e8 b9	. . .
	ld a,(lb7f0h)		;bd8c	3a f0 b7	: . .
	and a			;bd8f	a7		.
	jp z,lb9e8h		;bd90	ca e8 b9	. . .
	call sub_be0ch		;bd93	cd 0c be	. . .
	jp lba4ch		;bd96	c3 4c ba	. L .
lbd99h:
	ld h,b			;bd99	60		`
	ld c,a			;bd9a	4f		O
	ld b,a			;bd9b	47		G
	add hl,bc		;bd9c	09		.
	djnz lbda9h		;bd9d	10 0a		. .
	ld d,00eh		;bd9f	16 0e		. .
	ld h,018h		;bda1	26 18		& .
	rra			;bda3	1f		.
	ld c,01bh		;bda4	0e 1b		. .
	ld h,b			;bda6	60		`
	ld h,a			;bda7	67		g
	ld b,a			;bda8	47		G
lbda9h:
	add hl,bc		;bda9	09		.
	add hl,de		;bdaa	19		.
	dec d			;bdab	15		.
	ld a,(bc)		;bdac	0a		.
	ld (01b0eh),hl		;bdad	22 0e 1b	" . .
	ld h,026h		;bdb0	26 26		& &
lbdb2h:
	nop			;bdb2	00		.
sub_bdb3h:
	ld de,lbdb9h		;bdb3	11 b9 bd	. . .
	jp lb4ech		;bdb6	c3 ec b4	. . .
lbdb9h:
	ld c,b			;bdb9	48		H
	ld h,a			;bdba	67		g
	ld b,a			;bdbb	47		G
	rrca			;bdbc	0f		.
	add hl,de		;bdbd	19		.
	dec d			;bdbe	15		.
	ld a,(bc)		;bdbf	0a		.
	ld (01b0eh),hl		;bdc0	22 0e 1b	" . .
	inc e			;bdc3	1c		.
	ld h,001h		;bdc4	26 01		& .
	ld h,00ah		;bdc6	26 0a		& .
	rla			;bdc8	17		.
	dec c			;bdc9	0d		.
	ld h,002h		;bdca	26 02		& .
	djnz $+4		;bdcc	10 02		. .
	ld b,(hl)		;bdce	46		F
sub_bdcfh:
	ld de,04000h		;bdcf	11 00 40	. . @
	ld hl,0da00h		;bdd2	21 00 da	! . .
	ld b,000h		;bdd5	06 00		. .
	ld a,0c0h		;bdd7	3e c0		> .
lbdd9h:
	ex af,af'		;bdd9	08		.
	ld c,020h		;bdda	0e 20		.  
	push de			;bddc	d5		.
	ldir			;bddd	ed b0		. .
	pop de			;bddf	d1		.
	ld a,d			;bde0	7a		z
	inc d			;bde1	14		.
	cpl			;bde2	2f		/
	and 007h		;bde3	e6 07		. .
	jr nz,lbdf1h		;bde5	20 0a		  .
	ld a,e			;bde7	7b		{
	add a,020h		;bde8	c6 20		.  
	ld e,a			;bdea	5f		_
	jr c,lbdf1h		;bdeb	38 04		8 .
	ld a,d			;bded	7a		z
	sub 008h		;bdee	d6 08		. .
	ld d,a			;bdf0	57		W
lbdf1h:
	ex af,af'		;bdf1	08		.
	dec a			;bdf2	3d		=
	jr nz,lbdd9h		;bdf3	20 e4		  .
	ret			;bdf5	c9		.
sub_bdf6h:
	ld de,05800h		;bdf6	11 00 58	. . X
	ld hl,0d700h		;bdf9	21 00 d7	! . .
	ld bc,00300h		;bdfc	01 00 03	. . .
	ldir			;bdff	ed b0		. .
	ret			;be01	c9		.
sub_be02h:
	ld c,(hl)		;be02	4e		N
	ld a,(de)		;be03	1a		.
	ld (hl),a		;be04	77		w
	ld a,c			;be05	79		y
	ld (de),a		;be06	12		.
	inc hl			;be07	23		#
	inc de			;be08	13		.
	djnz sub_be02h		;be09	10 f7		. .
	ret			;be0b	c9		.
sub_be0ch:
	ld a,(lb7f0h)		;be0c	3a f0 b7	: . .
	and a			;be0f	a7		.
	ret z			;be10	c8		.
	ld de,(l9789h)		;be11	ed 5b 89 97	. [ . .
	push de			;be15	d5		.
	ld a,(lb7f2h)		;be16	3a f2 b7	: . .
	call sub_9779h		;be19	cd 79 97	. y .
	pop de			;be1c	d1		.
	ld bc,06100h		;be1d	01 00 61	. . a
	ld a,0b4h		;be20	3e b4		> .
lbe22h:
	ex af,af'		;be22	08		.
	ld a,(de)		;be23	1a		.
	push af			;be24	f5		.
	ld a,(bc)		;be25	0a		.
	ld (hl),a		;be26	77		w
	pop af			;be27	f1		.
	ld (bc),a		;be28	02		.
	inc hl			;be29	23		#
	inc de			;be2a	13		.
	inc bc			;be2b	03		.
	ex af,af'		;be2c	08		.
	dec a			;be2d	3d		=
	jr nz,lbe22h		;be2e	20 f2		  .
sub_be30h:
	ld hl,lb7e8h		;be30	21 e8 b7	! . .
	ld de,lb7f0h		;be33	11 f0 b7	. . .
	ld b,008h		;be36	06 08		. .
	call sub_be02h		;be38	cd 02 be	. . .
	ld hl,lb7a6h		;be3b	21 a6 b7	! . .
	ld de,lb7c8h		;be3e	11 c8 b7	. . .
	ld b,00ah		;be41	06 0a		. .
	call sub_be02h		;be43	cd 02 be	. . .
	ld a,(lb7e8h)		;be46	3a e8 b7	: . .
	and a			;be49	a7		.
	ret z			;be4a	c8		.
	ld a,(lb7e6h)		;be4b	3a e6 b7	: . .
	xor 001h		;be4e	ee 01		. .
	ld (lb7e6h),a		;be50	32 e6 b7	2 . .
	ret			;be53	c9		.
sub_be54h:
	call sub_9776h		;be54	cd 76 97	. v .
	push hl			;be57	e5		.
	call sub_b8b9h		;be58	cd b9 b8	. . .
	pop hl			;be5b	e1		.
	ld b,0b4h		;be5c	06 b4		. .
	ld c,000h		;be5e	0e 00		. .
lbe60h:
	ld a,(hl)		;be60	7e		~
	and 0a0h		;be61	e6 a0		. .
	jr nz,lbe66h		;be63	20 01		  .
	inc c			;be65	0c		.
lbe66h:
	inc hl			;be66	23		#
	djnz lbe60h		;be67	10 f7		. .
	ld a,c			;be69	79		y
	ld (lb7e9h),a		;be6a	32 e9 b7	2 . .
	ret			;be6d	c9		.
sub_be6eh:
	ld hl,lb7bch		;be6e	21 bc b7	! . .
	ld de,lb7eeh		;be71	11 ee b7	. . .
	ld b,003h		;be74	06 03		. .
lbe76h:
	ld a,(de)		;be76	1a		.
	cp (hl)			;be77	be		.
	ret c			;be78	d8		.
	jr nz,lbe7fh		;be79	20 04		  .
	dec de			;be7b	1b		.
	dec hl			;be7c	2b		+
	djnz lbe76h		;be7d	10 f7		. .
lbe7fh:
	ld de,0b7bah		;be7f	11 ba b7	. . .
	ld hl,lb7ech		;be82	21 ec b7	! . .
	ld bc,00003h		;be85	01 03 00	. . .
	ldir			;be88	ed b0		. .
	ret			;be8a	c9		.
sub_be8bh:
	ld hl,l8ee0h		;be8b	21 e0 8e	! . .
	ld a,(lb7eah)		;be8e	3a ea b7	: . .
	and 003h		;be91	e6 03		. .
	add a,a			;be93	87		.
	call sub_b5bbh		;be94	cd bb b5	. . .
	ld e,(hl)		;be97	5e		^
	inc hl			;be98	23		#
	ld d,(hl)		;be99	56		V
	ld (lbea1h+1),de	;be9a	ed 53 a2 be	. S . .
	ld hl,00f00h		;be9e	21 00 0f	! . .
lbea1h:
	ld de,lc015h		;bea1	11 15 c0	. . .
	call sub_b5c0h		;bea4	cd c0 b5	. . .
	call sub_b643h		;bea7	cd 43 b6	. C .
	ld a,010h		;beaa	3e 10		> .
	add a,l			;beac	85		.
	ld l,a			;bead	6f		o
	jr nz,lbea1h		;beae	20 f1		  .
	ld l,000h		;beb0	2e 00		. .
	ld a,h			;beb2	7c		|
	add a,010h		;beb3	c6 10		. .
	ld h,a			;beb5	67		g
	cp 0cfh			;beb6	fe cf		. .
	jr nz,lbea1h		;beb8	20 e7		  .
	ld hl,09f00h		;beba	21 00 9f	! . .
	ld de,l6b8fh		;bebd	11 8f 6b	. . k
	exx			;bec0	d9		.
	ld hl,lbf00h		;bec1	21 00 bf	! . .
	ld de,l6b3fh		;bec4	11 3f 6b	. ? k
	ld b,007h		;bec7	06 07		. .
lbec9h:
	push bc			;bec9	c5		.
	push de			;beca	d5		.
	ld l,000h		;becb	2e 00		. .
	call sub_b5c0h		;becd	cd c0 b5	. . .
	call sub_b643h		;bed0	cd 43 b6	. C .
	ld l,0f8h		;bed3	2e f8		. .
	call sub_b5c0h		;bed5	cd c0 b5	. . .
	call sub_b643h		;bed8	cd 43 b6	. C .
	pop de			;bedb	d1		.
	ld a,0c8h		;bedc	3e c8		> .
	add a,h			;bede	84		.
	ld h,a			;bedf	67		g
	exx			;bee0	d9		.
	pop bc			;bee1	c1		.
	djnz lbec9h		;bee2	10 e5		. .
	ld hl,0d941h		;bee4	21 41 d9	! A .
	ld a,004h		;bee7	3e 04		> .
	ld de,00020h		;bee9	11 20 00	.   .
	ld c,d			;beec	4a		J
lbeedh:
	ex af,af'		;beed	08		.
	ld b,01ch		;beee	06 1c		. .
	push hl			;bef0	e5		.
lbef1h:
	res 7,(hl)		;bef1	cb be		. .
	add hl,de		;bef3	19		.
	djnz lbef1h		;bef4	10 fb		. .
	pop hl			;bef6	e1		.
	push hl			;bef7	e5		.
	ld a,l			;bef8	7d		}
	add a,01dh		;bef9	c6 1d		. .
	ld l,a			;befb	6f		o
	ld b,01ch		;befc	06 1c		. .
lbefeh:
	res 0,(hl)		;befe	cb 86		. .
lbf00h:
	add hl,de		;bf00	19		.
	djnz lbefeh		;bf01	10 fb		. .
	pop hl			;bf03	e1		.
	ld b,007h		;bf04	06 07		. .
	add hl,bc		;bf06	09		.
	ex af,af'		;bf07	08		.
	dec a			;bf08	3d		=
	jr nz,lbeedh		;bf09	20 e2		  .
	ld de,00020h		;bf0b	11 20 00	.   .
	ld b,006h		;bf0e	06 06		. .
	ld hl,0d95eh		;bf10	21 5e d9	! ^ .
	ld a,(0d95dh)		;bf13	3a 5d d9	: ] .
lbf16h:
	ld (hl),a		;bf16	77		w
	add hl,de		;bf17	19		.
	djnz lbf16h		;bf18	10 fc		. .
	ld hl,lbfe7h		;bf1a	21 e7 bf	! . .
	exx			;bf1d	d9		.
	ld hl,00700h		;bf1e	21 00 07	! . .
lbf21h:
	exx			;bf21	d9		.
	ld e,(hl)		;bf22	5e		^
	inc hl			;bf23	23		#
	ld d,(hl)		;bf24	56		V
	inc hl			;bf25	23		#
	push de			;bf26	d5		.
	exx			;bf27	d9		.
	pop de			;bf28	d1		.
	call sub_b5c0h		;bf29	cd c0 b5	. . .
	call sub_b643h		;bf2c	cd 43 b6	. C .
	ld a,020h		;bf2f	3e 20		>  
	add a,l			;bf31	85		.
	ld l,a			;bf32	6f		o
	jr nc,lbf21h		;bf33	30 ec		0 .
	ld hl,0db01h		;bf35	21 01 db	! . .
sub_bf38h:
	ld de,lbff7h		;bf38	11 f7 bf	. . .
	ld b,01eh		;bf3b	06 1e		. .
lbf3dh:
	ld a,(de)		;bf3d	1a		.
	and (hl)		;bf3e	a6		.
	ld (hl),a		;bf3f	77		w
	inc l			;bf40	2c		,
	inc de			;bf41	13		.
	djnz lbf3dh		;bf42	10 f9		. .
	ld a,008h		;bf44	3e 08		> .
	ld (l9bc4h),a		;bf46	32 c4 9b	2 . .
	ld a,(lb7e8h)		;bf49	3a e8 b7	: . .
	dec a			;bf4c	3d		=
	jr z,lbf6ah		;bf4d	28 1b		( .
	ld b,a			;bf4f	47		G
	ld ix,l9bc2h		;bf50	dd 21 c2 9b	. ! . .
lbf54h:
	push bc			;bf54	c5		.
	call sub_b684h		;bf55	cd 84 b6	. . .
	call sub_9910h		;bf58	cd 10 99	. . .
	ld a,(l9bc4h)		;bf5b	3a c4 9b	: . .
	add a,010h		;bf5e	c6 10		. .
	cp 0e9h			;bf60	fe e9		. .
	jr nc,lbf67h		;bf62	30 03		0 .
	ld (l9bc4h),a		;bf64	32 c4 9b	2 . .
lbf67h:
	pop bc			;bf67	c1		.
	djnz lbf54h		;bf68	10 ea		. .
lbf6ah:
	ld a,(lb7e5h)		;bf6a	3a e5 b7	: . .
	cp 002h			;bf6d	fe 02		. .
	jr nz,lbf7bh		;bf6f	20 0a		  .
	ld ix,l9beeh		;bf71	dd 21 ee 9b	. ! . .
	call sub_b684h		;bf75	cd 84 b6	. . .
	call sub_9910h		;bf78	cd 10 99	. . .
lbf7bh:
	ld ix,l9bd4h		;bf7b	dd 21 d4 9b	. ! . .
	ld (ix+002h),01ch	;bf7f	dd 36 02 1c	. 6 . .
	ld (ix+001h),001h	;bf83	dd 36 01 01	. 6 . .
	call sub_b684h		;bf87	cd 84 b6	. . .
	call sub_9910h		;bf8a	cd 10 99	. . .
	inc (ix+001h)		;bf8d	dd 34 01	. 4 .
	ld (ix+002h),0cch	;bf90	dd 36 02 cc	. 6 . .
	call sub_b684h		;bf94	cd 84 b6	. . .
	call sub_9910h		;bf97	cd 10 99	. . .
	inc (ix+001h)		;bf9a	dd 34 01	. 4 .
	ld (ix+002h),078h	;bf9d	dd 36 02 78	. 6 . x
	call sub_b684h		;bfa1	cd 84 b6	. . .
	call sub_9910h		;bfa4	cd 10 99	. . .
	ld hl,(lb7a6h)		;bfa7	2a a6 b7	* . .
	exx			;bfaa	d9		.
	ld hl,lb7eeh		;bfab	21 ee b7	! . .
	call sub_96feh		;bfae	cd fe 96	. . .
	ld hl,(lb7c8h)		;bfb1	2a c8 b7	* . .
	exx			;bfb4	d9		.
	ld hl,lb7f6h		;bfb5	21 f6 b7	! . .
	call sub_96feh		;bfb8	cd fe 96	. . .
	ld hl,(0b7b6h)		;bfbb	2a b6 b7	* . .
	exx			;bfbe	d9		.
	ld hl,lb7bch		;bfbf	21 bc b7	! . .
	call sub_96feh		;bfc2	cd fe 96	. . .
	call sub_8d4ch		;bfc5	cd 4c 8d	. L .
	call sub_ade1h		;bfc8	cd e1 ad	. . .
	call sub_bfcfh		;bfcb	cd cf bf	. . .
	ret			;bfce	c9		.
sub_bfcfh:
	ld hl,0d721h		;bfcf	21 21 d7	! ! .
	ld b,017h		;bfd2	06 17		. .
	ld de,00020h		;bfd4	11 20 00	.   .
lbfd7h:
	res 6,(hl)		;bfd7	cb b6		. .
	add hl,de		;bfd9	19		.
	djnz lbfd7h		;bfda	10 fb		. .
	ld hl,0d722h		;bfdc	21 22 d7	! " .
	ld b,01dh		;bfdf	06 1d		. .
lbfe1h:
	res 6,(hl)		;bfe1	cb b6		. .
	inc l			;bfe3	2c		,
	djnz lbfe1h		;bfe4	10 fb		. .
	ret			;bfe6	c9		.
lbfe7h:
	call 0f56bh		;bfe7	cd 6b f5	. k .
	ld l,e			;bfea	6b		k
	dec e			;bfeb	1d		.
	ld l,h			;bfec	6c		l
	push af			;bfed	f5		.
	ld l,e			;bfee	6b		k
	ld b,l			;bfef	45		E
	ld l,h			;bff0	6c		l
	ld l,l			;bff1	6d		m
	ld l,h			;bff2	6c		l
	ld b,l			;bff3	45		E
	ld l,h			;bff4	6c		l
	sub l			;bff5	95		.
	ld l,h			;bff6	6c		l
lbff7h:
	nop			;bff7	00		.
	nop			;bff8	00		.
	inc bc			;bff9	03		.
	rst 38h			;bffa	ff		.
	rst 38h			;bffb	ff		.
	rst 38h			;bffc	ff		.
	ret nz			;bffd	c0		.
	nop			;bffe	00		.
	nop			;bfff	00		.
lc000h:
	nop			;c000	00		.
	inc bc			;c001	03		.
	rst 38h			;c002	ff		.
	rst 38h			;c003	ff		.
	rst 38h			;c004	ff		.
	ret nz			;c005	c0		.
lc006h:
	inc bc			;c006	03		.
	rst 38h			;c007	ff		.
	rst 38h			;c008	ff		.
	rst 38h			;c009	ff		.
	ret nz			;c00a	c0		.
	nop			;c00b	00		.
	nop			;c00c	00		.
	nop			;c00d	00		.
	inc bc			;c00e	03		.
	rst 38h			;c00f	ff		.
	rst 38h			;c010	ff		.
lc011h:
	rst 38h			;c011	ff		.
	ret nz			;c012	c0		.
	nop			;c013	00		.
	nop			;c014	00		.
lc015h:
	ld (bc),a		;c015	02		.
lc016h:
	djnz lc016h		;c016	10 fe		. .
	rst 38h			;c018	ff		.
	cp 0ffh			;c019	fe ff		. .
	cp 0ffh			;c01b	fe ff		. .
	cp 0ffh			;c01d	fe ff		. .
	cp 0ffh			;c01f	fe ff		. .
	cp 0ffh			;c021	fe ff		. .
	cp 0ffh			;c023	fe ff		. .
	ld sp,hl		;c025	f9		.
	ccf			;c026	3f		?
	rst 20h			;c027	e7		.
	rst 8			;c028	cf		.
	sbc a,a			;c029	9f		.
	di			;c02a	f3		.
	ld a,a			;c02b	7f		.
	defb 0fdh,0ffh,0feh ;illegal sequence	;c02c	fd ff fe	. . .
	ld a,a			;c02f	7f		.
	defb 0fdh,09fh,0f3h ;illegal sequence	;c030	fd 9f f3	. . .
	rst 20h			;c033	e7		.
	rst 8			;c034	cf		.
	ld sp,hl		;c035	f9		.
	ccf			;c036	3f		?
	ld (bc),a		;c037	02		.
	ld (bc),a		;c038	02		.
	ld b,(hl)		;c039	46		F
	ld b,(hl)		;c03a	46		F
	ld b,(hl)		;c03b	46		F
	ld b,(hl)		;c03c	46		F
sub_c03dh:
	ld a,l			;c03d	7d		}
	ld l,h			;c03e	6c		l
	ld h,000h		;c03f	26 00		& .
	add hl,hl		;c041	29		)
	add hl,hl		;c042	29		)
	add hl,hl		;c043	29		)
	add hl,hl		;c044	29		)
	add hl,hl		;c045	29		)
	srl a			;c046	cb 3f		. ?
	srl a			;c048	cb 3f		. ?
	srl a			;c04a	cb 3f		. ?
	ld c,a			;c04c	4f		O
	ld b,0dah		;c04d	06 da		. .
	add hl,bc		;c04f	09		.
	ret			;c050	c9		.
sub_c051h:
	ld c,l			;c051	4d		M
	ld a,h			;c052	7c		|
	and 0f8h		;c053	e6 f8		. .
	ld l,a			;c055	6f		o
	ld h,000h		;c056	26 00		& .
	add hl,hl		;c058	29		)
	add hl,hl		;c059	29		)
	srl c			;c05a	cb 39		. 9
	srl c			;c05c	cb 39		. 9
	srl c			;c05e	cb 39		. 9
	ld b,0d7h		;c060	06 d7		. .
	add hl,bc		;c062	09		.
	ret			;c063	c9		.
sub_c064h:
	ld ix,lc0b8h		;c064	dd 21 b8 c0	. ! . .
	ld b,004h		;c068	06 04		. .
	ld de,00007h		;c06a	11 07 00	. . .
lc06dh:
	ld a,(ix+000h)		;c06d	dd 7e 00	. ~ .
	and a			;c070	a7		.
	ret z			;c071	c8		.
	add ix,de		;c072	dd 19		. .
	djnz lc06dh		;c074	10 f7		. .
	ret			;c076	c9		.
sub_c077h:
	ld iy,(lb793h)		;c077	fd 2a 93 b7	. * . .
	ld a,(05c78h)		;c07b	3a 78 5c	: x \
	ld (lb7e7h),a		;c07e	32 e7 b7	2 . .
	ei			;c081	fb		.
	im 1			;c082	ed 56		. V
	ld a,(05cddh)		;c084	3a dd 5c	: . \
	and a			;c087	a7		.
	call nz,sub_974ah	;c088	c4 4a 97	. J .
	ld ix,lc0b8h		;c08b	dd 21 b8 c0	. ! . .
	ld b,005h		;c08f	06 05		. .
lc091h:
	push bc			;c091	c5		.
	ld a,(ix+000h)		;c092	dd 7e 00	. ~ .
	and a			;c095	a7		.
	call nz,sub_c0abh	;c096	c4 ab c0	. . .
	ld bc,00007h		;c099	01 07 00	. . .
	add ix,bc		;c09c	dd 09		. .
	pop bc			;c09e	c1		.
	djnz lc091h		;c09f	10 f0		. .
	ld a,(05c78h)		;c0a1	3a 78 5c	: x \
	ld b,a			;c0a4	47		G
	ld a,(lb7e7h)		;c0a5	3a e7 b7	: . .
	cp b			;c0a8	b8		.
	di			;c0a9	f3		.
	ret			;c0aa	c9		.
sub_c0abh:
	ld hl,lc0d9h		;c0ab	21 d9 c0	! . .
	add a,a			;c0ae	87		.
	ld e,a			;c0af	5f		_
	ld d,000h		;c0b0	16 00		. .
	add hl,de		;c0b2	19		.
	ld e,(hl)		;c0b3	5e		^
	inc hl			;c0b4	23		#
	ld d,(hl)		;c0b5	56		V
	ex de,hl		;c0b6	eb		.
	jp (hl)			;c0b7	e9		.
lc0b8h:
	nop			;c0b8	00		.
lc0b9h:
	nop			;c0b9	00		.
	nop			;c0ba	00		.
	nop			;c0bb	00		.
	nop			;c0bc	00		.
	nop			;c0bd	00		.
	nop			;c0be	00		.
	nop			;c0bf	00		.
sub_c0c0h:
	nop			;c0c0	00		.
	nop			;c0c1	00		.
	nop			;c0c2	00		.
	nop			;c0c3	00		.
	nop			;c0c4	00		.
	nop			;c0c5	00		.
sub_c0c6h:
	nop			;c0c6	00		.
	nop			;c0c7	00		.
	nop			;c0c8	00		.
	nop			;c0c9	00		.
	nop			;c0ca	00		.
	nop			;c0cb	00		.
	nop			;c0cc	00		.
lc0cdh:
	nop			;c0cd	00		.
	nop			;c0ce	00		.
	nop			;c0cf	00		.
	nop			;c0d0	00		.
	nop			;c0d1	00		.
	nop			;c0d2	00		.
	nop			;c0d3	00		.
lc0d4h:
	nop			;c0d4	00		.
	nop			;c0d5	00		.
	nop			;c0d6	00		.
	nop			;c0d7	00		.
	nop			;c0d8	00		.
lc0d9h:
	nop			;c0d9	00		.
	nop			;c0da	00		.
	di			;c0db	f3		.
	ret nz			;c0dc	c0		.
	ld bc,06fc1h		;c0dd	01 c1 6f	. . o
lc0e0h:
	pop bc			;c0e0	c1		.
	ld d,0c1h		;c0e1	16 c1		. .
	ld a,d			;c0e3	7a		z
lc0e4h:
	pop bc			;c0e4	c1		.
	xor b			;c0e5	a8		.
	pop bc			;c0e6	c1		.
	rst 8			;c0e7	cf		.
	pop bc			;c0e8	c1		.
	defb 0edh ;next byte illegal after ed	;c0e9	ed		.
	pop bc			;c0ea	c1		.
	nop			;c0eb	00		.
	jp nz,lc21dh		;c0ec	c2 1d c2	. . .
	dec (hl)		;c0ef	35		5
	jp nz,lc241h		;c0f0	c2 41 c2	. A .
	ld de,00844h		;c0f3	11 44 08	. D .
	call sub_c25ch		;c0f6	cd 5c c2	. \ .
	dec (ix+001h)		;c0f9	dd 35 01	. 5 .
	ld (ix+000h),000h	;c0fc	dd 36 00 00	. 6 . .
	ret			;c100	c9		.
sub_c101h:
	ld d,018h		;c101	16 18		. .
	ld e,030h		;c103	1e 30		. 0
	call sub_c25ch		;c105	cd 5c c2	. \ .
	xor a			;c108	af		.
	ld (lb7e7h),a		;c109	32 e7 b7	2 . .
	ld a,080h		;c10c	3e 80		> .
	ld (05c78h),a		;c10e	32 78 5c	2 x \
	ld (ix+000h),000h	;c111	dd 36 00 00	. 6 . .
	ret			;c115	c9		.
	ld c,009h		;c116	0e 09		. .
	ld e,014h		;c118	1e 14		. .
	call sub_c122h		;c11a	cd 22 c1	. " .
	ld (ix+000h),000h	;c11d	dd 36 00 00	. 6 . .
	ret			;c121	c9		.
sub_c122h:
	ld a,c			;c122	79		y
	xor e			;c123	ab		.
	add a,a			;c124	87		.
	ld b,a			;c125	47		G
	and 00fh		;c126	e6 0f		. .
	ld d,a			;c128	57		W
	ld a,b			;c129	78		x
	and 00ch		;c12a	e6 0c		. .
	add a,008h		;c12c	c6 08		. .
	ld b,a			;c12e	47		G
	call sub_c136h		;c12f	cd 36 c1	. 6 .
	dec c			;c132	0d		.
	jr nz,sub_c122h		;c133	20 ed		  .
	ret			;c135	c9		.
sub_c136h:
	ld a,010h		;c136	3e 10		> .
	out (0feh),a		;c138	d3 fe		. .
lc13ah:
	djnz lc13ah		;c13a	10 fe		. .
	xor a			;c13c	af		.
	out (0feh),a		;c13d	d3 fe		. .
	ld b,d			;c13f	42		B
lc140h:
	djnz lc140h		;c140	10 fe		. .
	ret			;c142	c9		.
sub_c143h:
	ld e,040h		;c143	1e 40		. @
	ld d,080h		;c145	16 80		. .
lc147h:
	ld b,c			;c147	41		A
	call sub_c136h		;c148	cd 36 c1	. 6 .
	inc d			;c14b	14		.
	inc c			;c14c	0c		.
	dec e			;c14d	1d		.
	jr nz,lc147h		;c14e	20 f7		  .
	ret			;c150	c9		.
sub_c151h:
	ld e,018h		;c151	1e 18		. .
	ld c,018h		;c153	0e 18		. .
	ld d,040h		;c155	16 40		. @
	jr lc147h		;c157	18 ee		. .
sub_c159h:
	ld e,0e0h		;c159	1e e0		. .
	ld c,e			;c15b	4b		K
	ld d,060h		;c15c	16 60		. `
lc15eh:
	ld b,c			;c15e	41		A
	call sub_c136h		;c15f	cd 36 c1	. 6 .
	dec d			;c162	15		.
	inc c			;c163	0c		.
	dec e			;c164	1d		.
	jr nz,lc15eh		;c165	20 f7		  .
	ret			;c167	c9		.
sub_c168h:
	ld c,0ffh		;c168	0e ff		. .
	ld e,03fh		;c16a	1e 3f		. ?
	jp sub_c122h		;c16c	c3 22 c1	. " .
	ld de,00466h		;c16f	11 66 04	. f .
	call sub_c25ch		;c172	cd 5c c2	. \ .
	ld (ix+000h),000h	;c175	dd 36 00 00	. 6 . .
	ret			;c179	c9		.
	ld e,(ix+002h)		;c17a	dd 5e 02	. ^ .
	ld d,001h		;c17d	16 01		. .
	call sub_c25ch		;c17f	cd 5c c2	. \ .
	inc (ix+002h)		;c182	dd 34 02	. 4 .
	inc (ix+002h)		;c185	dd 34 02	. 4 .
	inc (ix+002h)		;c188	dd 34 02	. 4 .
	ret			;c18b	c9		.
	ld a,018h		;c18c	3e 18		> .
	ld l,001h		;c18e	2e 01		. .
lc190h:
	ex af,af'		;c190	08		.
	ld de,001ffh		;c191	11 ff 01	. . .
lc194h:
	push de			;c194	d5		.
	call sub_c25ch		;c195	cd 5c c2	. \ .
	pop de			;c198	d1		.
	ld a,e			;c199	7b		{
	sub l			;c19a	95		.
	ld e,a			;c19b	5f		_
	jr nc,lc194h		;c19c	30 f6		0 .
	ld a,004h		;c19e	3e 04		> .
	add a,l			;c1a0	85		.
	ld l,a			;c1a1	6f		o
	ex af,af'		;c1a2	08		.
	dec a			;c1a3	3d		=
	jr nz,lc190h		;c1a4	20 ea		  .
	di			;c1a6	f3		.
	ret			;c1a7	c9		.
	ld a,(l8d48h)		;c1a8	3a 48 8d	: H .
	and 03fh		;c1ab	e6 3f		. ?
	add a,(ix+001h)		;c1ad	dd 86 01	. . .
	ld e,a			;c1b0	5f		_
	ld d,001h		;c1b1	16 01		. .
	call sub_c25ch		;c1b3	cd 5c c2	. \ .
	ld a,(ix+001h)		;c1b6	dd 7e 01	. ~ .
	add a,008h		;c1b9	c6 08		. .
	ld (ix+001h),a		;c1bb	dd 77 01	. w .
	cp 0a1h			;c1be	fe a1		. .
	jr z,lc1cah		;c1c0	28 08		( .
	cp 060h			;c1c2	fe 60		. `
	ret nz			;c1c4	c0		.
	ld (ix+001h),021h	;c1c5	dd 36 01 21	. 6 . !
	ret			;c1c9	c9		.
lc1cah:
	ld (ix+000h),000h	;c1ca	dd 36 00 00	. 6 . .
	ret			;c1ce	c9		.
	ld a,(ix+001h)		;c1cf	dd 7e 01	. ~ .
	and 003h		;c1d2	e6 03		. .
	jr nz,lc1e1h		;c1d4	20 0b		  .
	ld a,(ix+001h)		;c1d6	dd 7e 01	. ~ .
	add a,014h		;c1d9	c6 14		. .
	ld e,a			;c1db	5f		_
	ld d,003h		;c1dc	16 03		. .
	call sub_c25ch		;c1de	cd 5c c2	. \ .
lc1e1h:
	dec (ix+001h)		;c1e1	dd 35 01	. 5 .
	dec (ix+001h)		;c1e4	dd 35 01	. 5 .
	ret nz			;c1e7	c0		.
	ld (ix+000h),000h	;c1e8	dd 36 00 00	. 6 . .
	ret			;c1ec	c9		.
	ld a,(ix+001h)		;c1ed	dd 7e 01	. ~ .
	rra			;c1f0	1f		.
	rra			;c1f1	1f		.
	and 03fh		;c1f2	e6 3f		. ?
	add a,020h		;c1f4	c6 20		.  
	ld e,a			;c1f6	5f		_
	ld d,002h		;c1f7	16 02		. .
	call sub_c263h		;c1f9	cd 63 c2	. c .
	inc (ix+001h)		;c1fc	dd 34 01	. 4 .
	ret			;c1ff	c9		.
	ld a,(la85fh)		;c200	3a 5f a8	: _ .
	and a			;c203	a7		.
	ret nz			;c204	c0		.
	ld e,(ix+001h)		;c205	dd 5e 01	. ^ .
	ld d,001h		;c208	16 01		. .
	call sub_c25ch		;c20a	cd 5c c2	. \ .
	ld a,(ix+001h)		;c20d	dd 7e 01	. ~ .
	sub 00bh		;c210	d6 0b		. .
	ld (ix+001h),a		;c212	dd 77 01	. w .
	cp 010h			;c215	fe 10		. .
	ret nc			;c217	d0		.
	ld (ix+000h),000h	;c218	dd 36 00 00	. 6 . .
	ret			;c21c	c9		.
lc21dh:
	ld e,(ix+001h)		;c21d	dd 5e 01	. ^ .
	ld d,001h		;c220	16 01		. .
	call sub_c25ch		;c222	cd 5c c2	. \ .
	ld a,(ix+001h)		;c225	dd 7e 01	. ~ .
	add a,00bh		;c228	c6 0b		. .
	ld (ix+001h),a		;c22a	dd 77 01	. w .
	cp 0c1h			;c22d	fe c1		. .
	ret c			;c22f	d8		.
	ld (ix+000h),000h	;c230	dd 36 00 00	. 6 . .
	ret			;c234	c9		.
	ld c,004h		;c235	0e 04		. .
	ld e,00fh		;c237	1e 0f		. .
	call sub_c122h		;c239	cd 22 c1	. " .
	ld (ix+000h),000h	;c23c	dd 36 00 00	. 6 . .
	ret			;c240	c9		.
lc241h:
	ld e,030h		;c241	1e 30		. 0
	ld d,00ah		;c243	16 0a		. .
	call sub_c263h		;c245	cd 63 c2	. c .
	ld (ix+000h),000h	;c248	dd 36 00 00	. 6 . .
	ret			;c24c	c9		.
sub_c24dh:
	ld b,e			;c24d	43		C
	ei			;c24e	fb		.
	ld a,010h		;c24f	3e 10		> .
	out (0feh),a		;c251	d3 fe		. .
lc253h:
	djnz lc253h		;c253	10 fe		. .
	ld b,e			;c255	43		C
	xor a			;c256	af		.
	out (0feh),a		;c257	d3 fe		. .
lc259h:
	djnz lc259h		;c259	10 fe		. .
	ret			;c25b	c9		.
sub_c25ch:
	call sub_c24dh		;c25c	cd 4d c2	. M .
	dec d			;c25f	15		.
	jr nz,sub_c25ch		;c260	20 fa		  .
	ret			;c262	c9		.
sub_c263h:
	call sub_c24dh		;c263	cd 4d c2	. M .
	ld a,0f8h		;c266	3e f8		> .
	add a,e			;c268	83		.
	ld e,a			;c269	5f		_
	dec d			;c26a	15		.
	jr nz,sub_c263h		;c26b	20 f6		  .
	ret			;c26d	c9		.
	nop			;c26e	00		.
	nop			;c26f	00		.
	nop			;c270	00		.
	nop			;c271	00		.
	nop			;c272	00		.
	add hl,de		;c273	19		.
