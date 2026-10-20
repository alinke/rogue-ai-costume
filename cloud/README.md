# Photo service (optional)

Receives the photos from the costume and the scanner and serves them to the
landing page's galleries. It runs in **your own** Google Cloud project: one
Cloud Run service and one Cloud Storage bucket. Nothing here talks to anyone
else's project.

## What you need

- A Google Cloud project with billing enabled (a party's worth of photos costs
  cents; Cloud Vision face detection is free for the first 1,000 images a month).
- The `gcloud` CLI, logged in: `gcloud auth login`.

## Deploy

```bash
PROJECT=your-gcp-project ./deploy.sh
```

It enables the APIs it needs (Cloud Run, Cloud Build, Artifact Registry,
Storage, Vision), creates the bucket (public read, so the page can show the
photos), deploys the service, and prints:

- the **service URL** and **upload key** → `PHOTO_SERVICE_URL` and
  `PHOTO_UPLOAD_KEY` in `costume/src/secrets.h` and `scanner/src/secrets.h`;
- the **gallery feed** (`<service>/photos`) → `CONFIG.feed` in the landing page;
- your **admin page** (`<service>/admin?k=<ADMIN_KEY>`) — keep it private: it
  deletes photos, and shows the ones held back for having no face (SHOW puts one
  on the page).

The first run saves the project, bucket name and two random keys in `.keys`
(never committed); run `./deploy.sh` again any time to update the service.

## How it works

- `POST /upload?set=ai|lab` with header `X-Key: <upload key>` and a JPEG body.
  Without the key: 403. Optional `X-Label` (shown under the photo) and `X-Id`
  (a retry with the same id never makes a second copy).
- Photos where Cloud Vision finds no face are stored in `<set>-hidden/` instead
  of `<set>/` (the costume also sends a plain copy of the photo just for this
  check). The page never sees them unless you SHOW them.
- `GET /photos` lists both sets, newest first.
- `POST /clear` with header `X-Key: <admin key>` deletes every photo -- the
  costume's operator console CLEAR PHOTOS uses it if you give the costume the
  admin key (`PHOTO_ADMIN_KEY` in `costume/src/secrets.h`).

## After the party

```bash
gcloud storage cp -r gs://<bucket from .keys> ~/Downloads/rogue-ai-photos   # keep a copy (optional)
./teardown.sh
```

Deletes the service, the bucket with every photo, the container image, and turns
the Vision API back off (`DISABLE_VISION=0 ./teardown.sh` to leave it on). Then
set `CONFIG.feed = ""` on your page (the galleries disappear).
