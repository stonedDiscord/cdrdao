/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998  Andreas Mueller <mueller@daneb.ping.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/*
 * $Log: SampleManager.cc,v $
 * Revision 1.2  2000/02/20 23:34:54  llanero
 * fixed scsilib directory (files mising ?-()
 * ported xdao to 1.1.8 / gnome (MDI) app
 *
 * Revision 1.1.1.1  2000/02/05 01:39:52  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.6  1999/08/07 16:27:28  mueller
 * Applied patch from Yves Bastide:
 * * prefixing member function names with their class name in connect_to_method
 * * explicitly `const_cast'ing a cast to const
 *
 * Revision 1.5  1999/05/24 18:10:25  mueller
 * Adapted to new reading interface of 'trackdb'.
 *
 * Revision 1.4  1999/03/06 13:55:18  mueller
 * Adapted to Gtk-- version 0.99.1
 *
 * Revision 1.3  1999/01/30 19:45:43  mueller
 * Fixes for compilation with Gtk-- 0.11.1.
 *
 * Revision 1.1  1998/11/20 18:56:06  mueller
 * Initial revision
 *
 */

#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

#include <gtk--.h>
#include <gtk/gtk.h>

#include "SampleManager.h"

#include "TocEdit.h"
#include "guiUpdate.h"
#include "Toc.h"
#include "util.h"

#include "TrackDataScrap.h"

//llanero: class SampleManagerImpl : public Gtk_Signal_Base {
class SampleManagerImpl : public SigC::Object {
public:
  SampleManagerImpl(unsigned long);
  ~SampleManagerImpl();

  unsigned long blocking_;
  TocEdit *tocEdit_;
  TocReader tocReader_;

  short *leftNegSamples_;
  short *leftPosSamples_;
  short *rightNegSamples_;
  short *rightPosSamples_;
  long samplesSize_;
  long blocks_;
  unsigned long slength_;
  long chunk_;

  Sample *block_;
  long actBlock_;
  long endBlock_;
  long burstBlock_;
  unsigned long length_;
  gfloat percent_;
  gfloat percentStep_;
  int withGui_;

  int aborted_;

  Gtk::Window *progressWindow_;
  Gtk::ProgressBar *progressBar_;
  
  void getPeak(unsigned long start, unsigned long end,
	       short *leftNeg, short *leftPos,
	       short *rightNeg, short *rightPos);
  int scanToc(unsigned long start, unsigned long end);
  int readSamples();
  void abortAction();
  void reallocSamples(unsigned long maxSample);
  void removeSamples(unsigned long start, unsigned long end, TrackDataScrap *);
  void insertSamples(unsigned long pos, unsigned long len,
		     const TrackDataScrap *);
};

SampleManager::SampleManager(unsigned long blocking)
{
  impl_ = new SampleManagerImpl(blocking);
  
}

SampleManager::~SampleManager()
{
  delete impl_;
  impl_ = NULL;
}

unsigned long SampleManager::blocking() const
{
  return impl_->blocking_;
}

void SampleManager::setTocEdit(TocEdit *t)
{
  impl_->tocEdit_ = t;
  impl_->tocReader_.init(t->toc());

  impl_->blocks_ = 0;
}


int SampleManager::scanToc(unsigned long start, unsigned long end)
{
  return impl_->scanToc(start, end);
}

void SampleManager::getPeak(unsigned long start, unsigned long end,
			    short *leftNeg, short *leftPos,
			    short *rightNeg, short *rightPos)
{
  impl_->getPeak(start, end, leftNeg, leftPos, rightNeg, rightPos);
}

void SampleManager::removeSamples(unsigned long start, unsigned long end,
				  TrackDataScrap *scrap)
{
  impl_->removeSamples(start, end, scrap);
}

void SampleManager::insertSamples(unsigned long pos, unsigned long len,
				  const TrackDataScrap *scrap)
{
  impl_->insertSamples(pos, len, scrap);
}

SampleManagerImpl::SampleManagerImpl(unsigned long blocking) : tocReader_(NULL)
{
  blocking_ = blocking;
  tocEdit_ = NULL;

  leftNegSamples_ = leftPosSamples_ = NULL;
  rightNegSamples_ = rightPosSamples_ = NULL;
  samplesSize_ = 0;
  blocks_ = 0;
  slength_ = 0;

  block_ = new Sample[blocking_];
  actBlock_ = endBlock_ = burstBlock_ = 0;
  length_ = 0;
  withGui_ = 0;

  // allocate space in chunks of 40 minutes
  chunk_ = 40 * 60 * 75 * 588 / blocking;

  // create progress window
  Gtk::VBox *vbox = new Gtk::VBox();
  Gtk::Label *label = new Gtk::Label(string("Scanning Audio Data..."));
  
  vbox->pack_start(*label, FALSE, FALSE, 5);
  label->show();

  progressBar_ = new Gtk::ProgressBar();
  progressBar_->set_bar_style(GTK_PROGRESS_CONTINUOUS);
  progressBar_->set_orientation(GTK_PROGRESS_LEFT_TO_RIGHT);
  vbox->pack_start(*(progressBar_), FALSE, FALSE);
  progressBar_->show();

  Gtk::HBox *hbox = new Gtk::HBox();
  Gtk::Button *button = new Gtk::Button(string(" Abort "));
  hbox->pack_start(*button, TRUE, FALSE);
  button->show();
//llanero  connect_to_method(button->clicked, this, &SampleManagerImpl::abortAction);
  button->clicked.connect(slot(this, &SampleManagerImpl::abortAction));
  vbox->pack_start(*hbox, FALSE, FALSE, 5);
  hbox->show();

  progressWindow_ = new Gtk::Window(GTK_WINDOW_DIALOG);
//llanero  progressWindow_->add(vbox);
  progressWindow_->add(*vbox);
  progressWindow_->set_usize(200, 0);
  vbox->show();
}

SampleManagerImpl::~SampleManagerImpl()
{
  delete[] block_;

  delete[] leftNegSamples_;
  delete[] leftPosSamples_;
  delete[] rightNegSamples_;
  delete[] rightPosSamples_;

  tocEdit_ = NULL;
}

void SampleManagerImpl::getPeak(unsigned long start, unsigned long end,
				short *leftNeg, short *leftPos,
				short *rightNeg, short *rightPos)
{
  *leftNeg = *leftPos = 0;
  *rightNeg = *rightPos = 0;

  long startBlock = start / blocking_;
  long endBlock = end / blocking_;
  long i;

  if (startBlock >= blocks_ || endBlock >= blocks_)
    return;

  for (i = startBlock; i <= endBlock; i++) {
    assert(leftNegSamples_[i] <= 0);
    assert(rightNegSamples_[i] <= 0);
    assert(leftPosSamples_[i] >= 0);
    assert(rightPosSamples_[i] >= 0);
	
    if (leftNegSamples_[i] < *leftNeg)
      *leftNeg = leftNegSamples_[i];
	
    if (leftPosSamples_[i] > *leftPos)
      *leftPos = leftPosSamples_[i];
	
    if (rightNegSamples_[i] < *rightNeg)
      *rightNeg = rightNegSamples_[i];
	
    if (rightPosSamples_[i] > *rightPos)
      *rightPos = rightPosSamples_[i];
  }
}


int SampleManagerImpl::scanToc(unsigned long start, unsigned long end)
{
  long i;
  const Toc *toc;

  actBlock_ = start / blocking_;
  endBlock_ = end / blocking_;

  if (tocEdit_ == NULL || endBlock_ < actBlock_)
    return 1;

  toc = tocEdit_->toc();

  length_ = toc->length().samples();

  if (end >= length_)
    return 1;

  length_ -= actBlock_ * blocking_;

  reallocSamples(end);

  for (i = actBlock_; i <= endBlock_; i++) {
    leftNegSamples_[i] = rightNegSamples_[i] = -16000;
    leftPosSamples_[i] = rightPosSamples_[i] = 16000;
  }

  //message(0, "readSamples: %ld\n", endBlock_ - actBlock_ + 1);
  
  if (tocReader_.openData() != 0)
    return 2;

  if (tocReader_.seekSample(actBlock_ * blocking_) != 0) {
    tocReader_.closeData();
    return 2;
  }

  long len = endBlock_ - actBlock_ + 1;

  if (len < 2000) {
    burstBlock_ = len;
    percentStep_ = 1.0;
    withGui_ = 0;
  }
  else if (len < 10000) {
    burstBlock_ = len / 100;
    percentStep_ = 0.01;
    withGui_ = 1;
  }
  else {
    burstBlock_ = 500;
    percentStep_ = gfloat(burstBlock_) / gfloat(len);
    withGui_ = 1;
  }

  if (burstBlock_ == 0) 
    burstBlock_ = 1;

  percent_ = 0;
  aborted_ = 0;

  if (withGui_) {
    progressWindow_->show();

//llanero:    connect_to_method(Gtk_Main::idle(), this, &SampleManagerImpl::readSamples);
    Gtk::Main::idle.connect(slot(this, &SampleManagerImpl::readSamples));

    tocEdit_->blockEdit();
  }
  else {
    while (readSamples()) ;
  }

  return 0;
}

int SampleManagerImpl::readSamples()
{
  int j;
  long n;
  short lpossum, rpossum, lnegsum, rnegsum;
  int ret;
  long burstEnd = actBlock_ + burstBlock_;

  if (withGui_ && aborted_) {
    tocReader_.closeData();
    progressWindow_->hide();
    tocEdit_->unblockEdit();
    guiUpdate(UPD_SAMPLES);
    return 0;
  }

  for (; actBlock_ <= endBlock_ && actBlock_ < burstEnd && length_ > 0;
       actBlock_++) {
    n = length_ > blocking_ ? blocking_ : length_;
    if ((ret = tocReader_.readSamples(block_, n)) == n) {
      lpossum = lnegsum = rpossum = rnegsum = 0;
      for (j = 0; j < n; j++) {
	short d = block_[j].left();
	if (d > lpossum)
	  lpossum = d;
	if (d < lnegsum)
	  lnegsum = d;

	d = block_[j].right();
	if (d > rpossum)
	  rpossum = d;
	if (d < rnegsum)
	  rnegsum = d;
      }
      leftNegSamples_[actBlock_] = lnegsum;
      leftPosSamples_[actBlock_] = lpossum;
      rightNegSamples_[actBlock_] = rnegsum;
      rightPosSamples_[actBlock_] = rpossum;
    }
    else {
      message(-2, "Cannot read audio data: %ld - %ld.", n, ret);
      tocReader_.closeData();
      if (withGui_) {
	progressWindow_->hide();
      }
      tocEdit_->unblockEdit();
      guiUpdate(UPD_SAMPLES);
      return 0;
    }
    length_ -= n;
  }

  
  if (actBlock_ >= endBlock_ && actBlock_ < burstEnd) {
    tocReader_.closeData();
    if (withGui_) {
      progressWindow_->hide();
    }
    tocEdit_->unblockEdit();
    guiUpdate(UPD_SAMPLES);
    return 0;
  }

  percent_ += percentStep_;
  if (percent_ > 1.0) 
    percent_ = 1.0;

  if (withGui_) {
    progressBar_->set_percentage(percent_);
  }

  return 1;
}

void SampleManagerImpl::abortAction()
{
  aborted_ = 1;
}

void SampleManagerImpl::reallocSamples(unsigned long maxSample)
{
  long i;
  long maxBlock = (maxSample / blocking_) + 1;

  if (maxSample >= slength_)
    slength_ = maxSample + 1;

  if (maxBlock > blocks_)
    blocks_ = maxBlock;

  if (blocks_ > samplesSize_) {
    long newSize = samplesSize_ + chunk_;
    while (newSize < blocks_)
      newSize += chunk_;

    short *newLeftNeg = new short[newSize];
    short *newLeftPos = new short[newSize];
    short *newRightNeg = new short[newSize];
    short *newRightPos = new short[newSize];

    for (i = 0; i < samplesSize_; i++) {
      newLeftNeg[i] = leftNegSamples_[i];
      newLeftPos[i] = leftPosSamples_[i];
      newRightNeg[i] = rightNegSamples_[i];
      newRightPos[i] = rightPosSamples_[i];
    }
    samplesSize_ = newSize;

    delete[] leftNegSamples_;
    delete[] leftPosSamples_;
    delete[] rightNegSamples_;
    delete[] rightPosSamples_;
    leftNegSamples_ = newLeftNeg;
    leftPosSamples_ = newLeftPos;
    rightNegSamples_ = newRightNeg;
    rightPosSamples_ = newRightPos;
  }
}

void SampleManagerImpl::removeSamples(unsigned long start, unsigned long end,
				      TrackDataScrap *scrap)
{
  long i;
  long bstart;
  long oldBlocks = blocks_;
  long blen;
  unsigned long slen;

  if (start > end || end >= slength_)
    return;

  slen = end - start + 1;
  
  slength_ -= slen;

  if (slength_ == 0) {
    blocks_ = 0;
    return;
  }

  blocks_ = ((slength_ - 1) / blocking_) + 1;
  
  blen = oldBlocks - blocks_;
  
  bstart = start / blocking_;

  if (scrap != NULL)
    scrap->setPeaks(blen, &(leftNegSamples_[bstart]),
		    &(leftPosSamples_[bstart]), &(rightNegSamples_[bstart]),
		    &(rightPosSamples_[bstart]));

  if (blen > 0) {
    for (i = bstart; i < blocks_; i++) {
      leftNegSamples_[i] = leftNegSamples_[i + blen];
      leftPosSamples_[i] = leftPosSamples_[i + blen];
      rightNegSamples_[i] = rightNegSamples_[i + blen];
      rightPosSamples_[i] = rightPosSamples_[i + blen] ;
    }
  }
}

void SampleManagerImpl::insertSamples(unsigned long pos, unsigned long len,
				      const TrackDataScrap *scrap)
{
  long blen;
  long bpos;
  long oldBlocks;
  long i;

  if (len == 0)
    return;

  bpos = pos / blocking_;

  if (bpos > blocks_)
    bpos = blocks_;

  oldBlocks = blocks_;

  reallocSamples(slength_ + (len - 1));

  blen = blocks_ - oldBlocks;

  if (blen > 0) {
    for (i = blocks_ - 1; i >= bpos + blen; i--) {
      leftNegSamples_[i] = leftNegSamples_[i - blen];
      leftPosSamples_[i] = leftPosSamples_[i - blen];
      rightNegSamples_[i] = rightNegSamples_[i - blen];
      rightPosSamples_[i] = rightPosSamples_[i - blen] ;
    }

    // initialize the new region
    for (i = bpos; i < bpos + blen; i++) {
      leftNegSamples_[i] = -16000;
      leftPosSamples_[i] = 16000;
      rightNegSamples_[i] = -16000;
      rightPosSamples_[i] = 16000;
    }

    if (scrap != NULL)
      scrap->getPeaks(blen, &(leftNegSamples_[bpos]), &(leftPosSamples_[bpos]),
		      &(rightNegSamples_[bpos]), &(rightPosSamples_[bpos]));
  }
}
