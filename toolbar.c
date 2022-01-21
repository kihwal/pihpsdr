/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

#include <gtk/gtk.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef GPIO
#include "gpio.h"
#endif
#include "toolbar.h"
#include "mode.h"
#include "filter.h"
#include "bandstack.h"
#include "band.h"
#include "discovered.h"
#include "new_protocol.h"
#include "old_protocol.h"
#include "vfo.h"
#include "alex.h"
#include "agc.h"
#include "channel.h"
#include "wdsp.h"
#include "radio.h"
#include "receiver.h"
#include "transmitter.h"
#include "property.h"
#include "new_menu.h"
#include "button_text.h"
#include "ext.h"	
#ifdef CLIENT_SERVER
#include "client_server.h"
#endif

static int width;
static int height;

static GtkWidget *parent_window;
static GtkWidget *toolbar;

static GtkWidget *last_dialog;

static GtkWidget *sim_mox;
static GtkWidget *sim_band;
static GtkWidget *sim_mode;
static GtkWidget *sim_filter;
static GtkWidget *sim_noise;
static GtkWidget *sim_agc;
static GtkWidget *sim_bstack;
static GtkWidget *sim_ctun;
static GtkWidget *sim_mem;
static GtkWidget *sim_freq;
static GtkWidget *sim_rit;
static GtkWidget *sim_rit_inc;
static GtkWidget *sim_rit_dec;
static GtkWidget *sim_rit_cl;
static GtkWidget *sim_sat;
static GtkWidget *sim_rsat;
static GtkWidget *sim_tune;
static GtkWidget *sim_lock;
static GtkWidget *sim_split;
static GtkWidget *sim_atob;
static GtkWidget *sim_btoa;
static GtkWidget *sim_aswapb;
static GtkWidget *sim_duplex;

static GtkWidget *last_band;
static GtkWidget *last_bandstack;
static GtkWidget *last_mode;
static GtkWidget *last_filter;

static GdkRGBA white;
static GdkRGBA gray;

static gint rit_plus_timer=-1;
static gint rit_minus_timer=-1;
static gint xit_plus_timer=-1;
static gint xit_minus_timer=-1;

static gboolean rit_timer_cb(gpointer data) {
  int i=GPOINTER_TO_INT(data);
  vfo_rit(active_receiver->id,i);
  return TRUE;
}

static gboolean xit_timer_cb(gpointer data) {
  int i=GPOINTER_TO_INT(data);
  transmitter->xit+=(i*rit_increment);
  if(transmitter->xit>10000) transmitter->xit=10000;
  if(transmitter->xit<-10000) transmitter->xit=-10000;
  if(protocol==NEW_PROTOCOL) {
    schedule_high_priority();
  }
  g_idle_add(ext_vfo_update,NULL);
  return TRUE;
}

static void close_cb(GtkWidget *widget, gpointer data) {
  gtk_widget_destroy(last_dialog);
  last_dialog=NULL;
}

void band_cb(GtkWidget *widget, gpointer data) {
  start_band();
}

void bandstack_cb(GtkWidget *widget, gpointer data) {
  start_bandstack();
}

void mode_cb(GtkWidget *widget, gpointer data) {
  start_mode();
}

void filter_cb(GtkWidget *widget, gpointer data) {
  start_filter();
}

void agc_cb(GtkWidget *widget, gpointer data) {
  start_agc();
}

void noise_cb(GtkWidget *widget, gpointer data) {
  start_noise();
}

void ctun_cb(GtkWidget *widget, gpointer data) {
  int id=active_receiver->id;
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_ctun(client_socket,id,vfo[id].ctun==1?0:1);
  } else {
#endif
    vfo[id].ctun=vfo[id].ctun==1?0:1;
    if(!vfo[id].ctun) {
      vfo[id].offset=0;
    }
    vfo[id].ctun_frequency=vfo[id].frequency;
    set_offset(active_receiver,vfo[id].offset);
    g_idle_add(ext_vfo_update,NULL);
#ifdef CLIENT_SERVER
  }
#endif
}

static void atob_cb (GtkWidget *widget, gpointer data) {
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_vfo(client_socket,VFO_A_TO_B);
  } else {
#endif
    vfo_a_to_b();
#ifdef CLIENT_SERVER
  }
#endif
}

static void btoa_cb (GtkWidget *widget, gpointer data) {
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_vfo(client_socket,VFO_B_TO_A);
  } else {
#endif
    vfo_b_to_a();
#ifdef CLIENT_SERVER
  }
#endif
}

static void aswapb_cb (GtkWidget *widget, gpointer data) {
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_vfo(client_socket,VFO_A_SWAP_B);
  } else {
#endif
    vfo_a_swap_b();
#ifdef CLIENT_SERVER
  }
#endif
}

static void split_cb (GtkWidget *widget, gpointer data) {
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_split(client_socket,split==1?0:1);
  } else {
#endif
    g_idle_add(ext_split_toggle,NULL);
#ifdef CLIENT_SERVER
  }
#endif
}

static void duplex_cb (GtkWidget *widget, gpointer data) {
  if(can_transmit && !isTransmitting()) {
#ifdef CLIENT_SERVER
    if(radio_is_remote) {
      send_dup(client_socket,duplex==1?0:1);
    } else {
#endif
      duplex=(duplex==1)?0:1;
      g_idle_add(ext_set_duplex,NULL);
#ifdef CLIENT_SERVER
    }
#endif
  }
}

static void sat_cb (GtkWidget *widget, gpointer data) {
  int temp;
  if(can_transmit) {
    if(sat_mode==SAT_MODE) {
      temp=SAT_NONE;
    } else {
      temp=SAT_MODE;
    }
#ifdef CLIENT_SERVER
    if(radio_is_remote) {
      send_sat(client_socket,temp);
    } else {
#endif
      sat_mode=temp;
      g_idle_add(ext_vfo_update,NULL);
#ifdef CLIENT_SERVER
    }
#endif
  }
}

static void rsat_cb (GtkWidget *widget, gpointer data) {
  int temp;
  if(can_transmit) {
    if(sat_mode==RSAT_MODE) {
      temp=SAT_NONE;
    } else {
      temp=RSAT_MODE;
    }
#ifdef CLIENT_SERVER
    if(radio_is_remote) {
      send_sat(client_socket,temp);
    } else {
#endif
      sat_mode=temp;
      g_idle_add(ext_vfo_update,NULL);
#ifdef CLIENT_SERVER
    }
#endif
  }
}

static void rit_enable_cb(GtkWidget *widget, gpointer data) {
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_rit_update(client_socket,active_receiver->id);
  } else {
#endif
    vfo_rit_update(active_receiver->id);
#ifdef CLIENT_SERVER
  }
#endif
}

static void rit_cb(GtkWidget *widget, gpointer data) {
  int i=GPOINTER_TO_INT(data);
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_rit(client_socket,active_receiver->id,i);
  } else {
#endif
    vfo_rit(active_receiver->id,i);
    if(i<0) {
      rit_minus_timer=g_timeout_add(200,rit_timer_cb,GINT_TO_POINTER(i));
    } else {
      rit_plus_timer=g_timeout_add(200,rit_timer_cb,GINT_TO_POINTER(i));
    }
#ifdef CLIENT_SERVER
  }
#endif
}

static void rit_clear_cb(GtkWidget *widget, gpointer data) {
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_rit_clear(client_socket,active_receiver->id);
  } else {
#endif
    vfo_rit_clear(active_receiver->id);
#ifdef CLIENT_SERVER
  }
#endif
}

static void xit_enable_cb(GtkWidget *widget, gpointer data) {
  if(can_transmit) {
    transmitter->xit_enabled=transmitter->xit_enabled==1?0:1;
    if(protocol==NEW_PROTOCOL) {
      schedule_high_priority();
    }
    g_idle_add(ext_vfo_update,NULL);
  }
}

static void xit_cb(GtkWidget *widget, gpointer data) {
  if(can_transmit) {
    int i=GPOINTER_TO_INT(data);
    transmitter->xit+=i*rit_increment;
    if(transmitter->xit>10000) transmitter->xit=10000;
    if(transmitter->xit<-10000) transmitter->xit=-10000;
    if(protocol==NEW_PROTOCOL) {
      schedule_high_priority();
    }
    g_idle_add(ext_vfo_update,NULL);
    if(i<0) {
      xit_minus_timer=g_timeout_add(200,xit_timer_cb,GINT_TO_POINTER(i));
    } else {
      xit_plus_timer=g_timeout_add(200,xit_timer_cb,GINT_TO_POINTER(i));
    }
  }
}

static void xit_clear_cb(GtkWidget *widget, gpointer data) {
  if(can_transmit) {
    transmitter->xit=0;
    g_idle_add(ext_vfo_update,NULL);
  }
}

static void freq_cb(GtkWidget *widget, gpointer data) {
  start_vfo(active_receiver->id);
}

static void mem_cb(GtkWidget *widget, gpointer data) {
  start_store();
}

static void vox_cb(GtkWidget *widget, gpointer data) {
  vox_enabled=vox_enabled==1?0:1;
  g_idle_add(ext_vfo_update,NULL);
}

static void stop() {
  if(protocol==ORIGINAL_PROTOCOL) {
    old_protocol_stop();
  } else {
    new_protocol_stop();
  }
#ifdef GPIO
  gpio_close();
#endif
#ifdef WIRIINGPI
  gpio_close();
#endif
}

static void yes_cb(GtkWidget *widget, gpointer data) {
  stop();
  _exit(0);
}

static void exit_cb(GtkWidget *widget, gpointer data) {

  radioSaveState();

  GtkWidget *dialog=gtk_dialog_new_with_buttons("Exit",GTK_WINDOW(parent_window),GTK_DIALOG_DESTROY_WITH_PARENT,NULL,NULL);

  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *grid=gtk_grid_new();

  gtk_grid_set_column_homogeneous(GTK_GRID(grid),TRUE);
  gtk_grid_set_row_homogeneous(GTK_GRID(grid),TRUE);

  GtkWidget *label=gtk_label_new("Exit?");
  //gtk_widget_override_font(label, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(label);
  gtk_grid_attach(GTK_GRID(grid),label,1,0,1,1);

  GtkWidget *b_yes=gtk_button_new_with_label("Yes");
  //gtk_widget_override_font(b_yes, pango_font_description_from_string("Arial 18"));
  gtk_widget_show(b_yes);
  gtk_grid_attach(GTK_GRID(grid),b_yes,0,1,1,1);
  g_signal_connect(b_yes,"pressed",G_CALLBACK(yes_cb),NULL);

  gtk_container_add(GTK_CONTAINER(content),grid);
  GtkWidget *close_button=gtk_dialog_add_button(GTK_DIALOG(dialog),"Cancel",GTK_RESPONSE_OK);
  //gtk_widget_override_font(close_button, pango_font_description_from_string("Arial 18"));
  gtk_widget_show_all(dialog);

  g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy),
                           dialog);

  gtk_dialog_run(GTK_DIALOG(dialog));

}

void lock_cb(GtkWidget *widget, gpointer data) {
#ifdef CLIENT_SERVER
  if(radio_is_remote) {
    send_lock(client_socket,locked==1?0:1);
  } else {
#endif
    locked=locked==1?0:1;
    g_idle_add(ext_vfo_update,NULL);
#ifdef CLIENT_SERVER
  }
#endif
}

void mox_cb(GtkWidget *widget, gpointer data) {

  if(getTune()==1) {
    setTune(0);
  }
  if(getMox()==1) {
    setMox(0);
  } else if(canTransmit() || tx_out_of_band) {
    setMox(1);
  } else {
    transmitter_set_out_of_band(transmitter);
  }
  g_idle_add(ext_vfo_update,NULL);
}

void mox_update(int state) {
//fprintf(stderr,"mox_update: state=%d\n",state);
  if(getTune()==1) {
    setTune(0);
  }
  if(state) {
    if(canTransmit() || tx_out_of_band) {
      setMox(state);
    } else {
      transmitter_set_out_of_band(transmitter);
    }
  } else {
    setMox(state);
  }
  g_idle_add(ext_vfo_update,NULL);
}

void tune_cb(GtkWidget *widget, gpointer data) {
  if(getMox()==1) {
    setMox(0);
  }
  if(getTune()==1) {
    setTune(0);
  } else if(canTransmit() || tx_out_of_band) {
    setTune(1);
  } else {
    transmitter_set_out_of_band(transmitter);
  }
  g_idle_add(ext_vfo_update,NULL);
}

void tune_update(int state) {
  if(getMox()==1) {
    setMox(0);
  }
  if(state) {
    setTune(0);
    if(canTransmit() || tx_out_of_band) {
      setTune(1);
    } else {
      transmitter_set_out_of_band(transmitter);
    }
  } else {
    setTune(state);
  }
  g_idle_add(ext_vfo_update,NULL);
}


void noop_released_cb(GtkWidget *widget, gpointer data) {
}

void sim_rit_inc_pressed_cb(GtkWidget *widget, gpointer data) {
  if(rit_minus_timer==-1 && rit_plus_timer==-1) {
    rit_cb(widget,(void *)1);
  }
}

void sim_rit_inc_released_cb(GtkWidget *widget, gpointer data) {
  if(rit_plus_timer!=-1) {
    g_source_remove(rit_plus_timer);
    rit_plus_timer=-1;
  }
}

void sim_rit_dec_pressed_cb(GtkWidget *widget, gpointer data) {
  if(rit_minus_timer==-1 && rit_plus_timer==-1) {
    rit_cb(widget,(void *)-1);
  }
}

void sim_rit_dec_released_cb(GtkWidget *widget, gpointer data) {
  if(rit_minus_timer!=-1) {
    g_source_remove(rit_minus_timer);
    rit_minus_timer=-1;
  }
}

GtkWidget *toolbar_init(int my_width, int my_height, GtkWidget* parent) {
    width=my_width;
    height=my_height;
    parent_window=parent;

    int button_width=width/8;

    fprintf(stderr,"toolbar_init: width=%d height=%d button_width=%d\n", width,height,button_width);

    white.red=1.0;
    white.green=1.0;
    white.blue=1.0;
    white.alpha=0.0;

    gray.red=0.25;
    gray.green=0.25;
    gray.blue=0.25;
    gray.alpha=0.0;

    toolbar=gtk_grid_new();
    gtk_widget_set_size_request (toolbar, width, height);
    gtk_grid_set_column_homogeneous(GTK_GRID(toolbar),TRUE);

    // 1st row
    sim_mox=gtk_button_new_with_label("Mox");
    g_signal_connect(G_OBJECT(sim_mox),"clicked",G_CALLBACK(mox_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_mox,0,0,4,1);

    sim_band=gtk_button_new_with_label("Band");
    //gtk_widget_set_size_request (sim_band, button_width, 0);
    g_signal_connect(G_OBJECT(sim_band),"pressed",G_CALLBACK(band_cb),NULL);
    g_signal_connect(G_OBJECT(sim_band),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_band,4,0,4,1);

    sim_mode=gtk_button_new_with_label("Mode");
    g_signal_connect(G_OBJECT(sim_mode),"pressed",G_CALLBACK(mode_cb),NULL);
    g_signal_connect(G_OBJECT(sim_mode),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_mode,8,0,4,1);

    sim_filter=gtk_button_new_with_label("Filter");
    g_signal_connect(G_OBJECT(sim_filter),"pressed",G_CALLBACK(filter_cb),NULL);
    g_signal_connect(G_OBJECT(sim_filter),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_filter,12,0,4,1);

    sim_noise=gtk_button_new_with_label("Noise");
    g_signal_connect(G_OBJECT(sim_noise),"pressed",G_CALLBACK(noise_cb),NULL);
    g_signal_connect(G_OBJECT(sim_noise),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_noise,16,0,4,1);

    sim_agc=gtk_button_new_with_label("AGC");
    g_signal_connect(G_OBJECT(sim_agc),"pressed",G_CALLBACK(agc_cb),NULL);
    g_signal_connect(G_OBJECT(sim_agc),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_agc,20,0,4,1);

    sim_bstack=gtk_button_new_with_label("BStack");
    g_signal_connect(G_OBJECT(sim_bstack),"pressed",G_CALLBACK(bandstack_cb),NULL);
    g_signal_connect(G_OBJECT(sim_bstack),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_bstack,24,0,4,1);

    sim_ctun=gtk_button_new_with_label("CTUN");
    g_signal_connect(G_OBJECT(sim_ctun),"pressed",G_CALLBACK(ctun_cb),NULL);
    g_signal_connect(G_OBJECT(sim_ctun),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_ctun,28,0,4,1);

    // 2nd row
    sim_mem=gtk_button_new_with_label("Mem");
    g_signal_connect(G_OBJECT(sim_mem),"pressed",G_CALLBACK(mem_cb),NULL);
    g_signal_connect(G_OBJECT(sim_mem),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_mem,0,1,4,1);

    sim_freq=gtk_button_new_with_label("Freq");
    g_signal_connect(G_OBJECT(sim_freq),"pressed",G_CALLBACK(freq_cb),NULL);
    g_signal_connect(G_OBJECT(sim_freq),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_freq,4,1,4,1);

    sim_rit=gtk_button_new_with_label("RIT");
    g_signal_connect(G_OBJECT(sim_rit),"pressed",G_CALLBACK(rit_enable_cb),NULL);
    g_signal_connect(G_OBJECT(sim_rit),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_rit,8,1,4,1);

    sim_rit_inc=gtk_button_new_with_label("RIT+");
    g_signal_connect(G_OBJECT(sim_rit_inc),"pressed",G_CALLBACK(sim_rit_inc_pressed_cb),NULL);
    g_signal_connect(G_OBJECT(sim_rit_inc),"released",G_CALLBACK(sim_rit_inc_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_rit_inc,12,1,4,1);

    sim_rit_dec=gtk_button_new_with_label("RIT-");
    g_signal_connect(G_OBJECT(sim_rit_dec),"pressed",G_CALLBACK(sim_rit_dec_pressed_cb),NULL);
    g_signal_connect(G_OBJECT(sim_rit_dec),"released",G_CALLBACK(sim_rit_dec_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_rit_dec,16,1,4,1);

    sim_rit_cl=gtk_button_new_with_label("RIT CL");
    g_signal_connect(G_OBJECT(sim_rit_cl),"pressed",G_CALLBACK(rit_clear_cb),NULL);
    g_signal_connect(G_OBJECT(sim_rit_cl),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_rit_cl,20,1,4,1);

    sim_sat=gtk_button_new_with_label("SAT");
    g_signal_connect(G_OBJECT(sim_sat),"pressed",G_CALLBACK(sat_cb),NULL);
    g_signal_connect(G_OBJECT(sim_sat),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_sat,24,1,4,1);

    sim_rsat=gtk_button_new_with_label("RSAT");
    g_signal_connect(G_OBJECT(sim_rsat),"pressed",G_CALLBACK(rsat_cb),NULL);
    g_signal_connect(G_OBJECT(sim_rsat),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_rsat,28,1,4,1);

    // 3rd row
    sim_tune=gtk_button_new_with_label("Tune");
    g_signal_connect(G_OBJECT(sim_tune),"pressed",G_CALLBACK(tune_cb),NULL);
    g_signal_connect(G_OBJECT(sim_tune),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_tune,0,2,4,1);

    sim_lock=gtk_button_new_with_label("Lock");
    g_signal_connect(G_OBJECT(sim_lock),"pressed",G_CALLBACK(lock_cb),NULL);
    g_signal_connect(G_OBJECT(sim_lock),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_lock,4,2,4,1);

    sim_split=gtk_button_new_with_label("Split");
    g_signal_connect(G_OBJECT(sim_split),"pressed",G_CALLBACK(split_cb),NULL);
    g_signal_connect(G_OBJECT(sim_split),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_split,8,2,4,1);

    sim_atob=gtk_button_new_with_label("A > B");
    g_signal_connect(G_OBJECT(sim_atob),"pressed",G_CALLBACK(atob_cb),NULL);
    g_signal_connect(G_OBJECT(sim_atob),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_atob,12,2,4,1);

    sim_btoa=gtk_button_new_with_label("A < B");
    g_signal_connect(G_OBJECT(sim_btoa),"pressed",G_CALLBACK(btoa_cb),NULL);
    g_signal_connect(G_OBJECT(sim_btoa),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_btoa,16,2,4,1);

    sim_aswapb=gtk_button_new_with_label("A<>B");
    g_signal_connect(G_OBJECT(sim_aswapb),"pressed",G_CALLBACK(aswapb_cb),NULL);
    g_signal_connect(G_OBJECT(sim_aswapb),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_aswapb,20,2,4,1);

    sim_duplex=gtk_button_new_with_label("Duplex");
    g_signal_connect(G_OBJECT(sim_duplex),"pressed",G_CALLBACK(duplex_cb),NULL);
    g_signal_connect(G_OBJECT(sim_duplex),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_duplex,24,2,4,1);

/*
    sim_noise=gtk_button_new_with_label("Noise");
    g_signal_connect(G_OBJECT(sim_noise),"pressed",G_CALLBACK(noise_cb),NULL);
    g_signal_connect(G_OBJECT(sim_noise),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_noise,20,0,4,1);

    sim_agc=gtk_button_new_with_label("AGC");
    g_signal_connect(G_OBJECT(sim_agc),"pressed",G_CALLBACK(agc_cb),NULL);
    g_signal_connect(G_OBJECT(sim_agc),"released",G_CALLBACK(noop_released_cb),NULL);
    gtk_grid_attach(GTK_GRID(toolbar),sim_agc,24,0,4,1);
*/


    last_dialog=NULL;

    gtk_widget_show_all(toolbar);

  return toolbar;
}
