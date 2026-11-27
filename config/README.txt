NITRO_FX creates its data next to the .asi at runtime (no files needed to start):
  NITRO_FX\vehicles\model_<id>.nfx   active configuration of a vehicle model (autosaved working copy)
  NITRO_FX\saves\<SaveID>.nfx        named saves: any number per vehicle model (no per-model limit), one file each
  NITRO_FX\presets\*.nfx             legacy model-less presets from older builds (kept on disk, no longer used by the menu)

Save file (text, strict):
  NITRO_FX_SAVE 1 / id=<16 hex = Save ID = file name> / name=<text> / model=<vehicle model ID> / created=<unix time> / count=<emitters>
  mirror=, enabled=, preset=, then one e= line per emitter (up to 64):
  e=id,x,y,z,hdgX,hdgY,angleDeg,active,mirror,category,colorVar,speed,detail,glow,opacity,power   (floats written with 9 digits = exact)
  crc=<FNV-1a of everything above>
Any defect (bad CRC, malformed line, wrong id, count mismatch, NaN, unknown category...) rejects the whole file: it is renamed
to <SaveID>.nfx.corrupt and nothing is applied. Autosave files (vehicles\) stay lenient: bad lines are skipped.
