#include <linux/module.h>
#include <linux/export.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#ifdef CONFIG_MARVELL_MEDIA_MMU
#include <linux/m4u.h>
#endif
#include <media/videobuf2-dma-sg.h>
#include <media/videobuf2-dma-contig.h>

#include <linux/dma-contiguous.h>
#include <asm/cacheflush.h>

#include <media/b52socisp/b52socisp-mdev.h>
#include <media/b52socisp/b52socisp-vdev.h>

#define _SHARED_BUFFER_ 1

static int trace = 1;
module_param(trace, int, 0644);
MODULE_PARM_DESC(trace,
		"how many trace do you want to see? (0-4):"
		"0 - mute "
		"1 - only actual errors"
		"2 - milestone log"
		"3 - briefing log"
		"4 - detailed log");

static struct isp_format_desc isp_format_table[] = {
	{ V4L2_MBUS_FMT_SBGGR10_2X8_PADHI_LE, V4L2_PIX_FMT_SBGGR16, 16, 1},
	{ V4L2_MBUS_FMT_SGBRG10_2X8_PADHI_LE, V4L2_PIX_FMT_SGBRG16, 16, 1},
	{ V4L2_MBUS_FMT_SGRBG10_2X8_PADHI_LE, V4L2_PIX_FMT_SGRBG16, 16, 1},
	{ V4L2_MBUS_FMT_SRGGB10_2X8_PADHI_LE, V4L2_PIX_FMT_SRGGB16, 16, 1},
	{ V4L2_MBUS_FMT_SBGGR10_1X10,	V4L2_PIX_FMT_SBGGR10,	10,	1},
	{ V4L2_MBUS_FMT_SBGGR8_1X8,	V4L2_PIX_FMT_SBGGR8,	8,	1},
	{ V4L2_MBUS_FMT_UYVY8_1X16,	V4L2_PIX_FMT_UYVY,	16,	1},
	{ V4L2_MBUS_FMT_NV21_1X12,	V4L2_PIX_FMT_NV21M,	12,	2},
	{ V4L2_MBUS_FMT_NV12_1X12,	V4L2_PIX_FMT_NV12M,	12,	2},
	{ V4L2_MBUS_FMT_YUYV8_1_5X8,	V4L2_PIX_FMT_YUV420M,	12,	3},
	{ V4L2_MBUS_FMT_YVYU8_1_5X8,	V4L2_PIX_FMT_YVU420M,	12,	3},
};


struct rcv_buffer {
	struct list_head	hook;
	phys_addr_t		pa;
	void			*va;
	struct page		*page;
	size_t			size;
};

struct list_head __vnode_rcv_buffer_list = LIST_HEAD_INIT(__vnode_rcv_buffer_list);
DEFINE_MUTEX(rcv_lock);

static int __alloc_vnode_rcv_buffer(struct rcv_buffer *rcv, size_t size)
{
	struct page *page = NULL;
	int order;

	size = PAGE_ALIGN(size);
	order = get_order(size);

	if (IS_ENABLED(CONFIG_CMA))
		page = dma_alloc_from_contiguous(NULL, size >> PAGE_SHIFT, order);

	if (!page) {
		rcv->size = 0;
		rcv->pa = (phys_addr_t)NULL;
		rcv->va = NULL;
		return -ENOMEM;
	}

	rcv->pa = (phys_addr_t)page_to_phys(page);
	rcv->va = (void *)phys_to_virt(rcv->pa);
	rcv->size = size;

	return 0;
}

int alloc_vnode_rcv_buffer(size_t size)
{
	struct rcv_buffer *rcv;
	int ret;

	rcv = kzalloc(sizeof(*rcv), GFP_KERNEL);
	if (WARN_ON(rcv == NULL))
		return -ENOMEM;
	INIT_LIST_HEAD(&rcv->hook);

	ret = __alloc_vnode_rcv_buffer(rcv, size);
	if (WARN_ON(ret < 0))
		return -ENOMEM;

	mutex_lock(&rcv_lock);
	list_add_tail(&rcv->hook, &__vnode_rcv_buffer_list);
	mutex_unlock(&rcv_lock);
	d_inf(1, "------Allocate debug buffe with size %lu", rcv->size);
	return 0;
}

struct rcv_buffer *vnode_get_rcv_buffer(struct isp_vnode *vnode)
{
	struct rcv_buffer *rcv;
	size_t	size = 0;
	int i;

	if (vnode->rcv)
		return vnode->rcv;

	for (i = 0; i < vnode->format.fmt.pix_mp.num_planes; i++)
		size += vnode->format.fmt.pix_mp.plane_fmt[i].sizeimage;

	mutex_lock(&rcv_lock);
	if (list_empty(&__vnode_rcv_buffer_list)) {
		mutex_unlock(&rcv_lock);
		WARN_ON(1);
		return NULL;
	}

	list_for_each_entry(rcv, &__vnode_rcv_buffer_list, hook)
		if (size <= rcv->size)
			goto find;

	mutex_unlock(&rcv_lock);
	WARN_ON(1);
	return NULL;
find:
#if _SHARED_BUFFER_
	/* If all vnode use the same reserve buffer, nothing to do */
#else
	/* If allocate reserve buffer for each vnode, enable this */
	list_del_init(&rcv->hook);
#endif
	vnode->rcv = rcv;
	mutex_unlock(&rcv_lock);
	d_inf(2, "------Got buffer done for %s @ [%08llX - %08llX]",
		vnode->vdev.name,
		vnode->rcv->pa,
		vnode->rcv->pa + vnode->rcv->size);
	return rcv;
}

void vnode_put_rcv_buffer(struct isp_vnode *vnode)
{
	struct rcv_buffer *rcv = vnode->rcv;

	if (rcv == NULL)
		return;

	mutex_lock(&rcv_lock);
#if _SHARED_BUFFER_
#else
	list_add_tail(&rcv->hook, &__vnode_rcv_buffer_list);
#endif
	vnode->rcv = NULL;
	mutex_unlock(&rcv_lock);
	d_inf(2, "------Put buffer done for %s", vnode->vdev.name);
}

__maybe_unused void vnode_format_rcv_buffer(struct isp_vnode *vnode)
{
	struct rcv_buffer *rcv = vnode->rcv;
	memset(rcv->va, 0, rcv->size);
	d_inf(3, "------Format buffer done for %s", vnode->vdev.name);
}

__maybe_unused void vnode_verify_rcv_buffer(struct isp_vnode *vnode)
{
	size_t i, start, end;
	__u64 *ptr;
	int in_err = 0;

	if (WARN_ON(vnode->rcv == NULL))
		return;
	ptr = vnode->rcv->va;

	for (i = 0; i < vnode->rcv->size / sizeof(*ptr); i += 8) {
		if ((ptr[i + 0] | ptr[i + 1] | ptr[i + 2] | ptr[i + 3]) ||
			(ptr[i + 4] | ptr[i + 5] | ptr[i + 6] | ptr[i + 7]))  {
			if (!in_err) {
				in_err = 1;
				start = i * sizeof(*ptr);
			}
		} else {
			if (in_err) {
				in_err = 0;
				end = i * sizeof(*ptr);
				d_inf(1, "YOU ARE LUCKY: %s error at [%08llX .. %08llX]",
					vnode->vdev.name,
					vnode->rcv->pa + start / sizeof(*ptr),
					vnode->rcv->pa + end / sizeof(*ptr));
				memset(vnode->rcv->va, 0, vnode->rcv->size);
			}
		}
	}
	d_inf(3, "---Verify Buffer Done for %s @ %08llX",
		vnode->vdev.name, vnode->rcv->pa);
}

static int isp_video_calc_mplane_sizeimage(
		struct v4l2_pix_format_mplane *pix_mp, int idx)
{
	unsigned int width = pix_mp->width;
	unsigned int height = pix_mp->height;
	unsigned int pitch;
	int ret = 0;

	switch (pix_mp->pixelformat) {
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SBGGR16:
	case V4L2_PIX_FMT_SGBRG16:
	case V4L2_PIX_FMT_SGRBG16:
	case V4L2_PIX_FMT_SRGGB16:
		pitch = isp_format_table[idx].bpp * width >> 3;
		pix_mp->plane_fmt[0].bytesperline = pitch;
		pix_mp->plane_fmt[0].sizeimage = pitch * height;
		if (width & 0x07) {
			WARN(1, "width need 8 align\n");
			ret = -EINVAL;
		}
		break;
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
		pitch = width;
		pix_mp->plane_fmt[0].bytesperline = pitch;
		pix_mp->plane_fmt[0].sizeimage = pitch * height;

		pitch = width >> 1;
		pix_mp->plane_fmt[1].bytesperline = pitch;
		pix_mp->plane_fmt[1].sizeimage =  pitch * height / 2;

		pix_mp->plane_fmt[2].bytesperline = pitch;
		pix_mp->plane_fmt[2].sizeimage = pitch * height / 2;
		if (width & 0x1f) {
			WARN(1, "width need 32 align\n");
			ret = -EINVAL;
		}
		break;
	case V4L2_PIX_FMT_NV21M:
	case V4L2_PIX_FMT_NV12M:
		pitch = width;
		pix_mp->plane_fmt[0].bytesperline = pitch;
		pix_mp->plane_fmt[0].sizeimage = pitch * height;

		pitch = width >> 1;
		pix_mp->plane_fmt[1].bytesperline = pitch;
		pix_mp->plane_fmt[1].sizeimage =  pitch * height;
		if (width & 0x0f) {
			WARN(1, "width need 16 align\n");
			ret = -EINVAL;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int isp_video_mbus_to_pix(const struct v4l2_mbus_framefmt *mbus,
					struct v4l2_pix_format_mplane *pix_mp)
{
	unsigned int i;

	memset(pix_mp, 0, sizeof(*pix_mp));

	for (i = 0; i < ARRAY_SIZE(isp_format_table); ++i) {
		if (isp_format_table[i].code == mbus->code)
			break;
	}
	if (WARN_ON(i >= ARRAY_SIZE(isp_format_table)))
		return -EINVAL;

	pix_mp->width = mbus->width;
	pix_mp->height = mbus->height;
	pix_mp->pixelformat = isp_format_table[i].pixelformat;
	pix_mp->colorspace = mbus->colorspace;
	pix_mp->field = mbus->field;
	pix_mp->num_planes = isp_format_table[i].num_planes;

	return isp_video_calc_mplane_sizeimage(pix_mp, i);
}

static int isp_video_pix_to_mbus(struct v4l2_pix_format_mplane *pix_mp,
				  struct v4l2_mbus_framefmt *mbus)
{
	unsigned int i;

	memset(mbus, 0, sizeof(*mbus));

	for (i = 0; i < ARRAY_SIZE(isp_format_table); ++i) {
		if (isp_format_table[i].pixelformat == pix_mp->pixelformat)
			break;
	}

	if (WARN_ON(i >= ARRAY_SIZE(isp_format_table)))
		return -EINVAL;

	/*
	 * NO! we don't care about how does user space setup plane number,
	 * instead, user space should check plane number according to our setup
	 */
	pix_mp->num_planes = isp_format_table[i].num_planes;

	mbus->code = isp_format_table[i].code;
	mbus->width = pix_mp->width;
	mbus->height = pix_mp->height;
	mbus->colorspace = pix_mp->colorspace;
	mbus->field = pix_mp->field;

	return 0;
}

struct isp_vnode *ispsd_get_video(struct isp_subdev *ispsd)
{
	struct media_entity *vdev_me, *me = &ispsd->subdev.entity;
	int i;

	if (ispsd == NULL || (!ispsd->single))
		return NULL;

	if ((ispsd->sd_type != ISD_TYPE_DMA_OUT)
		&& (ispsd->sd_type != ISD_TYPE_DMA_IN))
		return NULL;

	for (i = 0; i < me->num_links; i++) {
		struct media_link *link = &me->links[i];

		if (link->source->entity == me)
			vdev_me = link->sink->entity;
		else
			vdev_me = link->source->entity;
		if (vdev_me->type == MEDIA_ENT_T_DEVNODE_V4L)
			goto find;
	}
	return NULL;
find:
	return video_get_drvdata(media_entity_to_video_device(vdev_me));
}

struct isp_subdev *video_get_ispsd(struct isp_vnode *vnode)
{
	struct media_entity *ispsd_me, *me = &vnode->vdev.entity;
	struct isp_subdev *ispsd;
	int i;

	if (unlikely(vnode == NULL))
		return NULL;

	for (i = 0; i < me->num_links; i++) {
		struct media_link *link = &me->links[i];
		if (link->source->entity == me)
			ispsd_me = link->sink->entity;
		else
			ispsd_me = link->source->entity;
		if (ispsd_me->type == MEDIA_ENT_T_V4L2_SUBDEV)
			goto find;
	}
	return NULL;
find:
	ispsd = me_to_ispsd(ispsd_me);
	if (!ispsd || (!ispsd->single) ||
		((ispsd->sd_type != ISD_TYPE_DMA_OUT) &&
		(ispsd->sd_type != ISD_TYPE_DMA_IN)))
		return NULL;
	return ispsd;
}

struct isp_videobuf *isp_vnode_get_idle_buffer(struct isp_vnode *vnode)
{
	struct isp_videobuf *buf = NULL;
	unsigned long flags;

	spin_lock_irqsave(&vnode->vb_lock, flags);

	if (vnode->idle_buf_cnt) {
		buf = list_first_entry(&vnode->idle_buf,
			struct isp_videobuf, hook);
		list_del_init(&buf->hook);
		vnode->idle_buf_cnt--;
	}

	spin_unlock_irqrestore(&vnode->vb_lock, flags);
	return buf;	/* should be written to DMA H/W */
}
EXPORT_SYMBOL(isp_vnode_get_idle_buffer);

struct isp_videobuf *isp_vnode_get_busy_buffer(struct isp_vnode *vnode)
{
	struct isp_videobuf *buf = NULL;
	unsigned long flags;

	spin_lock_irqsave(&vnode->vb_lock, flags);

	if (!vnode_buf_can_export(vnode))
		goto unlock;

	if (vnode->busy_buf_cnt) {
		buf = list_first_entry(&vnode->busy_buf,
			struct isp_videobuf, hook);
		list_del_init(&buf->hook);
		vnode->busy_buf_cnt--;
	}

	if (vnode->idle_buf_cnt + vnode->busy_buf_cnt < vnode->hw_min_buf) {
		int ret;
		ret = blocking_notifier_call_chain(&vnode->notifier.head,
					VDEV_NOTIFY_STM_OFF, vnode);
		d_inf(3, "%s: hardware stream off", vnode->vdev.name);
		if (ret < 0)
			d_inf(1, "%s: failed to stream off in kernel thread: %d",
				vnode->vdev.name, ret);
	}

unlock:
	spin_unlock_irqrestore(&vnode->vb_lock, flags);

	return buf;
}
EXPORT_SYMBOL(isp_vnode_get_busy_buffer);

struct isp_videobuf *isp_vnode_find_busy_buffer(struct isp_vnode *vnode,
						__u8 pos)
{
	struct isp_videobuf *buf;
	unsigned long flags;

	spin_lock_irqsave(&vnode->vb_lock, flags);
	if (pos >= vnode->busy_buf_cnt) {
		spin_unlock_irqrestore(&vnode->vb_lock, flags);
		return NULL;
	}
	list_for_each_entry(buf, &vnode->busy_buf, hook) {
		if (pos <= 0)
			break;
		pos--;
	}
	spin_unlock_irqrestore(&vnode->vb_lock, flags);
	return buf;
}
EXPORT_SYMBOL(isp_vnode_find_busy_buffer);

int isp_vnode_put_busy_buffer(struct isp_vnode *vnode,
				struct isp_videobuf *buf)
{
	unsigned long flags;

	if (buf == NULL || !list_empty(&buf->hook))
		return -EPERM;

	spin_lock_irqsave(&vnode->vb_lock, flags);
	list_add_tail(&buf->hook, &vnode->busy_buf);
	vnode->busy_buf_cnt++;
	spin_unlock_irqrestore(&vnode->vb_lock, flags);
	return 0;	/* should be written to DMA H/W */
}
EXPORT_SYMBOL(isp_vnode_put_busy_buffer);

int isp_vnode_export_buffer(struct isp_videobuf *buf)
{
	if (buf == NULL || !list_empty(&buf->hook))
		return -EPERM;

	/* Actually deliver the hot frame to VB */
	v4l2_get_timestamp(&buf->vb.v4l2_buf.timestamp);
	vb2_buffer_done(&buf->vb, VB2_BUF_STATE_DONE);
	return 0;
}
EXPORT_SYMBOL(isp_vnode_export_buffer);

/********************************** VB2 ops ***********************************/

static int isp_vbq_setup(struct vb2_queue *vq, const struct v4l2_format *fmt,
			unsigned int *num_buffers, unsigned int *num_planes,
			unsigned int sizes[], void *alloc_ctxs[])
{
	struct isp_vnode *vnode = vb2_get_drv_priv(vq);
	struct v4l2_pix_format_mplane *pix_mp = &vnode->format.fmt.pix_mp;
	int i;

	if (vnode->mmu_enabled && vq->memory != V4L2_MEMORY_DMABUF) {
		d_inf(1, "error MMU only support DMABUF\n");
		return -EINVAL;
	}

	if (vnode->mmu_enabled)
		vq->mem_ops	= &vb2_dma_sg_memops;
	else
		vq->mem_ops	= &vb2_dma_contig_memops;

	if (*num_buffers < vnode->min_buf_cnt)
		*num_buffers = vnode->min_buf_cnt;

	*num_planes = pix_mp->num_planes;
	for (i = 0; i < VIDEO_MAX_PLANES; i++) {
		sizes[i] = ALIGN(pix_mp->plane_fmt[i].sizeimage,
				ALIGN_SIZE);
		alloc_ctxs[i] = vnode->alloc_ctx;
	}

	INIT_LIST_HEAD(&vnode->busy_buf);
	INIT_LIST_HEAD(&vnode->idle_buf);

	vnode->busy_buf_cnt = 0;
	vnode->idle_buf_cnt = 0;

	return 0;
}

static int isp_vb_finish(struct vb2_buffer *vb)
{
	struct isp_vnode *vnode = container_of(vb->vb2_queue,
					struct isp_vnode, vq);
	struct isp_videobuf *isp_vb = container_of(vb, struct isp_videobuf, vb);
	int i;

	/* For each image plane, cleanup before expose buffer */
	for (i = 0; i < vnode->format.fmt.pix_mp.num_planes; i++) {
		if (vnode->mmu_enabled) {
#ifdef CONFIG_MARVELL_MEDIA_MMU
			struct msc2_ch_info *info = &isp_vb->ch_info[i];
			if (info->bdt) {
				m4u_put_bdt(info->bdt);
				info->bdt = NULL;
		    }
#endif
		}
	}

	/* For each data plane, clean kernel mapping of buffer addr */
	for (i = vnode->format.fmt.pix_mp.num_planes; i < vb->num_planes; i++) {
		if (vb->planes[i].dbuf == NULL)
			continue;
		if (isp_vb->va[i] == NULL)
			continue;
		dma_buf_kunmap(vb->planes[i].dbuf,
			PAGE_ALIGN(vb2_plane_size(vb, i)) / PAGE_SIZE,
			isp_vb->va[i]);
		dma_buf_end_cpu_access(vb->planes[i].dbuf,
			0, vb2_plane_size(vb, i), DMA_FROM_DEVICE);
		isp_vb->va[i] = NULL;
	}

	return 0;
}

static void isp_vb_cleanup(struct vb2_buffer *vb)
{
	isp_vb_finish(vb);
}

static int isp_vb_prepare(struct vb2_buffer *vb)
{
	struct isp_vnode *vnode = container_of(vb->vb2_queue,
					struct isp_vnode, vq);
	struct isp_videobuf *isp_vb = container_of(vb, struct isp_videobuf, vb);
	struct v4l2_pix_format_mplane *pix_mp = &vnode->format.fmt.pix_mp;
	dma_addr_t dma_handle;
	unsigned long size;
	int i, ret = 0;

	if (vb->num_planes < pix_mp->num_planes)
		return -EINVAL;
	for (i = 0; i < vnode->format.fmt.pix_mp.num_planes; i++) {
		struct msc2_ch_info *info = &isp_vb->ch_info[i];
		/*
		 * if use MMU, dma_address is just a translation reference
		 * DE said bit[11:0] for PA and VA better be the same, maybe
		 * try that?
		 */
		if (vnode->mmu_enabled)
			dma_handle = vnode->rcv->pa;
		else
			dma_handle = vb2_dma_contig_plane_dma_addr(&isp_vb->vb, i) +
				vb->v4l2_planes[i].data_offset;
		BUG_ON(!dma_handle);

		size = vb2_plane_size(vb, i);
		vb2_set_plane_payload(vb, i, size);
		info->daddr = dma_handle;

		if (vnode->mmu_enabled) {
#ifdef CONFIG_MARVELL_MEDIA_MMU
			/* if Descriptor Chain had been setup, skip */
			if (info->bdt && info->bdt->dscr_cnt)
				continue;

			info->bdt = m4u_get_bdt(vb->planes[i].dbuf,
						vb2_dma_sg_plane_desc(vb, i)->sgt,
						vb->v4l2_planes[i].data_offset,
						size, 0x3f, PAGE_SIZE - 1);
			if (info->bdt == NULL) {
				ret = -EAGAIN;
				goto err_meta;
			}
#endif
		}
	}

	if (vnode->mmu_enabled)
		mb();

	for (i = vnode->format.fmt.pix_mp.num_planes; i < vb->num_planes; i++) {
		size = vb2_plane_size(vb, i);
		vb2_set_plane_payload(vb, i, size);

		if (vb->planes[i].dbuf) {
			ret = dma_buf_begin_cpu_access(vb->planes[i].dbuf,
				0, size, DMA_FROM_DEVICE);
			if (ret < 0)
				goto err_meta;
			isp_vb->va[i] = dma_buf_kmap(vb->planes[i].dbuf, 0) +
					vb->v4l2_planes[i].data_offset;
		}
	}

	INIT_LIST_HEAD(&isp_vb->hook);
	return 0;

err_meta:
	return ret;
}

static void isp_vb_queue(struct vb2_buffer *vb)
{
	struct isp_vnode *vnode = container_of(vb->vb2_queue,
					struct isp_vnode, vq);
	struct isp_videobuf *isp_vb = container_of(vb, struct isp_videobuf, vb);
	unsigned long flags;
	int strm_on = 0;

	spin_lock_irqsave(&vnode->vb_lock, flags);
	if ((vnode->idle_buf_cnt + vnode->busy_buf_cnt + 1 == vnode->hw_min_buf)
		&& (vnode->state >= ISP_VNODE_ST_WORK))
		strm_on = 1;
	list_add_tail(&isp_vb->hook, &vnode->idle_buf);
	vnode->idle_buf_cnt++;
	spin_unlock_irqrestore(&vnode->vb_lock, flags);

	blocking_notifier_call_chain(&vnode->notifier.head,
				     VDEV_NOTIFY_QBUF, vnode);

	d_inf(4, "%s: buffer<%d> pushed into driver", vnode->vdev.name,
		vb->v4l2_buf.index);

	if (strm_on) {
		d_inf(3, "%s: hardware stream on", vnode->vdev.name);
		blocking_notifier_call_chain(&vnode->notifier.head,
				     VDEV_NOTIFY_STM_ON, vnode);
	}
	return;
}

static void isp_vb_lock(struct vb2_queue *vq)
{
	struct isp_vnode *vnode = vb2_get_drv_priv(vq);
	mutex_lock(&vnode->vdev_lock);
	return;
}

static void isp_vb_unlock(struct vb2_queue *vq)
{
	struct isp_vnode *vnode = vb2_get_drv_priv(vq);
	mutex_unlock(&vnode->vdev_lock);
	return;
}

static struct vb2_ops isp_vnode_vb2_ops = {
	.queue_setup	= isp_vbq_setup,
	.buf_prepare	= isp_vb_prepare,
	.buf_queue	= isp_vb_queue,
	.buf_cleanup	= isp_vb_cleanup,
	.buf_finish	= isp_vb_finish,
	.wait_prepare	= isp_vb_unlock,
	.wait_finish	= isp_vb_lock,
};

static int isp_vnode_vb2_init(struct vb2_queue *vq, struct isp_vnode *vnode)
{
	vq->type	= vnode->buf_type;
	vq->io_modes	= VB2_USERPTR | VB2_DMABUF;
	vq->drv_priv	= vnode;
	vq->ops		= &isp_vnode_vb2_ops;
	vq->mem_ops	= &vb2_dma_contig_memops;
	vq->buf_struct_size	= sizeof(struct isp_videobuf);
	vq->timestamp_type	= V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;

	return vb2_queue_init(vq);
}

/********************************* V4L2 APIs *********************************/

static int __maybe_unused isp_vnode_querycap(struct file *file, void *fh,
				struct v4l2_capability *cap)
{
	struct isp_vnode *vnode = video_drvdata(file);
	struct isp_subdev *ispsd = me_to_ispsd(
			media_entity_remote_source(&vnode->pad)->entity);
	int ret = 0;

	if (ispsd == NULL)
		return -EPIPE;
	strncpy(cap->driver, "socisp-vdev", sizeof(cap->driver));
	strncpy(cap->card, ispsd->subdev.name, sizeof(cap->card));

	if (V4L2_TYPE_IS_OUTPUT(vnode->buf_type)) {
		cap->capabilities = V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			V4L2_CAP_STREAMING;
	} else {
		cap->capabilities = V4L2_CAP_VIDEO_CAPTURE_MPLANE |
			V4L2_CAP_STREAMING;
	}

	return ret;
}

static int isp_vnode_get_format(struct file *file, void *fh,
				struct v4l2_format *format)
{
	struct isp_vnode *vnode = video_drvdata(file);

	if (format->type != vnode->buf_type)
		return -EINVAL;
	*format = vnode->format;
	return 0;
}

static int isp_vnode_set_format(struct file *file, void *fh,
				struct v4l2_format *format)
{
	struct isp_vnode *vnode = video_drvdata(file);
	struct isp_subdev *ispsd;
	struct media_pad *pad;
	struct v4l2_subdev_format sd_fmt;
	int ret = 0;

	pad = media_entity_remote_source(&vnode->pad);
	if (!pad) {
		d_inf(1, "link was disabled in force, vnode hasn't linked any entity\n");
		return -EINVAL;
	}
	ispsd =  me_to_ispsd(pad->entity);

	if (format->type != vnode->buf_type)
		return -EINVAL;

	ret = isp_video_pix_to_mbus(&format->fmt.pix_mp, &sd_fmt.format);
	if (ret)
		goto out;

	d_inf(2, "%s: set mbus fmt:%X(%d, %d)", vnode->vdev.name,
		sd_fmt.format.code, sd_fmt.format.width, sd_fmt.format.height);
	sd_fmt.pad = media_entity_remote_source(&vnode->pad)->index;
	sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	ret = isp_entity_try_set_fmt(&ispsd->subdev.entity, &sd_fmt);
	if (ret)
		goto out;

	ret = isp_video_mbus_to_pix(&sd_fmt.format, &format->fmt.pix_mp);
	if (ret)
		goto out;

	vnode->format = *format;
out:
	return ret;
}

static int isp_vnode_try_format(struct file *file, void *fh,
				struct v4l2_format *format)
{
	struct isp_vnode *vnode = video_drvdata(file);
	struct isp_subdev *ispsd = me_to_ispsd(
			media_entity_remote_source(&vnode->pad)->entity);
	struct v4l2_subdev_format sd_fmt;
	int ret = 0;

	if (format->type != vnode->buf_type)
		return -EINVAL;

	ret = isp_video_pix_to_mbus(&format->fmt.pix_mp, &sd_fmt.format);
	if (ret)
		goto out;

	sd_fmt.pad = media_entity_remote_source(&vnode->pad)->index;
	sd_fmt.which = V4L2_SUBDEV_FORMAT_TRY;
	ret = isp_entity_try_set_fmt(&ispsd->subdev.entity, &sd_fmt);
	if (ret)
		goto out;

	ret = isp_video_mbus_to_pix(&sd_fmt.format, &format->fmt.pix_mp);
	if (ret)
		goto out;

out:
	return ret;
}

static int isp_vnode_reqbufs(struct file *file, void *fh,
				struct v4l2_requestbuffers *rb)
{
	struct isp_vnode *vnode = video_drvdata(file);
	return vb2_reqbufs(&vnode->vq, rb);
}

static int isp_vnode_querybuf(struct file *file, void *fh,
				struct v4l2_buffer *vb)
{
	struct isp_vnode *vnode = video_drvdata(file);
	return vb2_querybuf(&vnode->vq, vb);
}

static int isp_vnode_qbuf(struct file *file, void *fh, struct v4l2_buffer *vb)
{
	struct isp_vnode *vnode = video_drvdata(file);
	if (vb->memory == V4L2_MEMORY_USERPTR && vb->length == 0)
		return -EINVAL;
	return vb2_qbuf(&vnode->vq, vb);
}

static int isp_vnode_dqbuf(struct file *file, void *fh,	struct v4l2_buffer *vb)
{
	struct isp_vnode *vnode = video_drvdata(file);
	return vb2_dqbuf(&vnode->vq, vb, file->f_flags & O_NONBLOCK);
}

static int isp_vnode_streamon(struct file *file, void *fh,
				enum v4l2_buf_type type)
{
	struct isp_vnode *vnode = video_drvdata(file);
	struct media_pad *pad = media_entity_remote_source(&vnode->pad);
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(pad->entity);
	int ret = 0;

	if (type != vnode->buf_type)
		return -EINVAL;

	mutex_lock(&vnode->st_lock);
	if (vnode->state >= ISP_VNODE_ST_WORK) {
		ret = -EBUSY;
		goto err_exit;
	}

	/* TODO: Apply final format: maybe default or user defined */
	/* 1st: if not set format, get default format */
	/* 2nd: apply format to pipeline */
	INIT_LIST_HEAD(&vnode->busy_buf);
	vnode->busy_buf_cnt = 0;
	ret = vb2_streamon(&vnode->vq, vnode->buf_type);
	if (ret < 0)
		goto err_exit;

	if (vnode->idle_buf_cnt >= vnode->hw_min_buf) {
		ret = v4l2_subdev_call(sd, video, s_stream, 1);
		if (ret < 0)
			goto err_stream;

		ret = blocking_notifier_call_chain(&vnode->notifier.head,
			VDEV_NOTIFY_STM_ON, vnode);
		if (ret < 0)
			goto err_notifier;
	}
	vnode->state = ISP_VNODE_ST_WORK;
	mutex_unlock(&vnode->st_lock);
	d_inf(2, "%s: stream on", vnode->vdev.name);
	return ret;

err_notifier:
	v4l2_subdev_call(sd, video, s_stream, 0);
err_stream:
	vb2_streamoff(&vnode->vq, vnode->buf_type);
err_exit:
	mutex_unlock(&vnode->st_lock);
	d_inf(1, "%s: failed to stream on ", vnode->vdev.name);
	return ret;
}

static int isp_vnode_streamoff(struct file *file, void *fh,
				enum v4l2_buf_type type)
{
	struct isp_vnode *vnode = video_drvdata(file);
	struct media_pad *pad;
	struct v4l2_subdev *sd;
	unsigned long flags;
	int ret;

	if (type != vnode->buf_type)
		return -EINVAL;

	mutex_lock(&vnode->st_lock);
	if (vnode->state < ISP_VNODE_ST_WORK) {
		mutex_unlock(&vnode->st_lock);
		return 0;
	}

	pad = media_entity_remote_source(&vnode->pad);
	if (unlikely(WARN_ON(pad == NULL))) {
		mutex_unlock(&vnode->st_lock);
		return -EPIPE;
	}
	sd = media_entity_to_v4l2_subdev(pad->entity);

	ret = blocking_notifier_call_chain(&vnode->notifier.head,
			VDEV_NOTIFY_STM_OFF, vnode);
	ret |= v4l2_subdev_call(sd, video, s_stream, 0);

	spin_lock_irqsave(&vnode->vb_lock, flags);
	ret |= vb2_streamoff(&vnode->vq, type);
	INIT_LIST_HEAD(&vnode->idle_buf);
	INIT_LIST_HEAD(&vnode->busy_buf);
	vnode->busy_buf_cnt = 0;
	vnode->idle_buf_cnt = 0;
	spin_unlock_irqrestore(&vnode->vb_lock, flags);

	vnode->state = ISP_VNODE_ST_LINK;
	mutex_unlock(&vnode->st_lock);
	d_inf(2, "%s: stream off", vnode->vdev.name);
	return ret;
}

static int isp_vnode_g_input(struct file *file, void *fh, unsigned int *input)
{
	*input = 0;
	return 0;
}

static int isp_vnode_s_input(struct file *file, void *fh, unsigned int input)
{
	return input == 0 ? 0 : -EINVAL;
}

static const struct v4l2_ioctl_ops isp_vnode_ioctl_ops = {
/*	.vidioc_querycap		= isp_vnode_querycap,*/
	.vidioc_g_fmt_vid_cap_mplane	= isp_vnode_get_format,
	.vidioc_s_fmt_vid_cap_mplane	= isp_vnode_set_format,
	.vidioc_try_fmt_vid_cap_mplane	= isp_vnode_try_format,
	.vidioc_g_fmt_vid_out_mplane	= isp_vnode_get_format,
	.vidioc_s_fmt_vid_out_mplane	= isp_vnode_set_format,
	.vidioc_try_fmt_vid_out_mplane	= isp_vnode_try_format,
	.vidioc_reqbufs			= isp_vnode_reqbufs,
	.vidioc_querybuf		= isp_vnode_querybuf,
	.vidioc_qbuf			= isp_vnode_qbuf,
	.vidioc_dqbuf			= isp_vnode_dqbuf,
	.vidioc_g_input			= isp_vnode_g_input,
	.vidioc_s_input			= isp_vnode_s_input,
	.vidioc_streamon		= isp_vnode_streamon,
	.vidioc_streamoff		= isp_vnode_streamoff,
};

static int isp_vnode_close(struct file *file)
{
	struct isp_vnode *vnode = video_drvdata(file);
	struct media_pad *pad = media_entity_remote_source(&vnode->pad);
	struct v4l2_subdev *sd;

	/* Just in case streamoff is not called yet */
	isp_vnode_streamoff(file, NULL, vnode->buf_type);

	mutex_lock(&vnode->st_lock);
	if (vnode->state == ISP_VNODE_ST_IDLE)
		goto exit;

	if (pad) {
		sd = media_entity_to_v4l2_subdev(pad->entity);
		v4l2_subdev_call(sd, core, init, 0);
	}
	vb2_queue_release(&vnode->vq);
	blocking_notifier_call_chain(&vnode->notifier.head,
			VDEV_NOTIFY_CLOSE, vnode);
	vnode->state = ISP_VNODE_ST_IDLE;
	/* OK, safe now, release graph lock */
	mutex_unlock(&vnode->link_lock);
	vnode->file = NULL;
	vnode_put_rcv_buffer(vnode);

exit:
	mutex_unlock(&vnode->st_lock);
	return 0;
}

static int isp_vnode_open(struct file *file)
{
	struct isp_vnode *vnode = video_drvdata(file);
	struct media_pad *pad;
	struct v4l2_subdev *sd = NULL;
	int ret = 0;

	if (mutex_lock_interruptible(&vnode->st_lock))
		return -ERESTARTSYS;
	if (vnode->state > ISP_VNODE_ST_IDLE) {
		mutex_unlock(&vnode->st_lock);
		d_inf(1, "%s: vdev already in use ", vnode->vdev.name);
		return -EBUSY;
	}

	/*
	 * Careful! In the following part we're doing something related to the
	 * media link, so MUST hold the link lock, otherwize another process
	 * may mess it up at our back.
	 */
	mutex_lock(&vnode->link_lock);

	pad = media_entity_remote_source(&vnode->pad);
	/* If backend link not enabled, reject open */
	if (pad == NULL) {
		d_inf(1, "vdev node not connected to any media entity");
		ret = -EPIPE;
		goto err_pipe;
	}

	/* Notify back-end entity about vdev open */
	sd = media_entity_to_v4l2_subdev(pad->entity);
	if (subdev_has_fn(sd, core, init)) {
		ret = v4l2_subdev_call(sd, core, init, 1);
		if (ret < 0)
			goto err_sd;
	}

	ret = blocking_notifier_call_chain(&vnode->notifier.head,
			VDEV_NOTIFY_OPEN, vnode);
	if (ret < 0)
		goto err_notify;

	/* Video buffer and vdev initialize */
	ret = isp_vnode_vb2_init(&vnode->vq, vnode);
	if (ret < 0)
		goto err_vb;
	memset(&vnode->format, 0, sizeof(vnode->format));
	vnode->format.type = vnode->buf_type;
	vnode->state = ISP_VNODE_ST_LINK;
	vnode->file = file;
	ret = v4l2_ctrl_handler_setup(&vnode->ctrl_handler);
	if (ret < 0)
		goto err_handler;
	mutex_unlock(&vnode->st_lock);
	vnode_get_rcv_buffer(vnode);
	return ret;

err_handler:
	/* Nothing to do for VB2 de-init */
err_vb:
	blocking_notifier_call_chain(&vnode->notifier.head,
			VDEV_NOTIFY_CLOSE, vnode);
err_notify:
	if (sd)
		v4l2_subdev_call(sd, core, init, 0);
err_sd:
err_pipe:
	mutex_unlock(&vnode->link_lock);
	mutex_unlock(&vnode->st_lock);
	return ret;
}

static unsigned int isp_vnode_poll(struct file *file, poll_table *wait)
{
	struct isp_vnode *vnode = video_drvdata(file);
	struct vb2_queue *vbq = &vnode->vq;
	if ((vbq == NULL) || (vbq->num_buffers == 0 && vbq->fileio == NULL)
		|| list_empty(&vbq->queued_list))
		return POLLERR;
	return vb2_poll(vbq, file, wait);
}

static ssize_t isp_vnode_read(struct file *file, char __user *buf, size_t count,
			loff_t *ppos)
{
	struct isp_vnode *vnode = video_drvdata(file);
	d_inf(1, "%s: read() is empty", vnode->vdev.entity.name);
	count = 0;
	return 0;
}

static int isp_vnode_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct isp_vnode *vnode = video_drvdata(file);
	struct vb2_queue *vbq = &vnode->vq;
	return vb2_mmap(vbq, vma);
}

static const struct v4l2_file_operations isp_vnode_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= video_ioctl2,
	.open		= &isp_vnode_open,
	.release	= &isp_vnode_close,
	.poll		= &isp_vnode_poll,
	.read		= &isp_vnode_read,
	.mmap		= &isp_vnode_mmap,
};

#define VNODE_DEF_W	640
#define VNODE_DEF_H	480

static struct v4l2_pix_format_mplane default_output_format = {
	.width		= VNODE_DEF_W,
	.height		= VNODE_DEF_H,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.plane_fmt	= {
		[0] = {
			.sizeimage	= VNODE_DEF_W * VNODE_DEF_H * 2,
			.bytesperline	= VNODE_DEF_W * 2,
		},
	},
	.num_planes	= 1,
};

/* initialize isp video node for a dma-capabile agent, dir: O-Output, 1-Input */
int isp_vnode_add(struct isp_vnode *vnode, struct v4l2_device *vdev,
			int dir, int vdev_nr)
{
	int ret = 0;

	spin_lock_init(&vnode->vb_lock);
	mutex_init(&vnode->vdev_lock);
	mutex_init(&vnode->st_lock);
	mutex_init(&vnode->link_lock);
	INIT_LIST_HEAD(&vnode->idle_buf);
	INIT_LIST_HEAD(&vnode->busy_buf);
	vnode->idle_buf_cnt = vnode->busy_buf_cnt = 0;
	if (dir == 0) {
		vnode->buf_type		= V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		vnode->pad.flags	= MEDIA_PAD_FL_SINK;
		vnode->mode		= ISP_VNODE_MODE_STREAM;
	} else {
		vnode->buf_type		= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		vnode->pad.flags	= MEDIA_PAD_FL_SOURCE;
		vnode->vdev.vfl_dir	= VFL_DIR_TX;
		vnode->mode		= ISP_VNODE_MODE_SINGLE;
	}
	ret = media_entity_init(&vnode->vdev.entity, 1, &vnode->pad, 0);
	if (ret < 0)
		goto exit;
	vnode->vdev.entity.group_id = MEDIA_ENT_T_DEVNODE_V4L;
	vnode->vdev.vfl_type	= VFL_TYPE_GRABBER;
	vnode->vdev.release	= video_device_release_empty;
	vnode->vdev.ioctl_ops	= &isp_vnode_ioctl_ops;
	vnode->vdev.fops	= &isp_vnode_fops;
	vnode->vdev.lock	= &vnode->vdev_lock;
	vnode->vdev.v4l2_dev	= vdev;
	vnode->vdev.ctrl_handler = &vnode->ctrl_handler;
	video_set_drvdata(&vnode->vdev, vnode);
	ret = video_register_device(&vnode->vdev, VFL_TYPE_GRABBER, vdev_nr);
	/* FIXME: hard coding, not all DMA device needs contig mem, try pass
	 * alloc_ctx handle, and call */
	if (vnode->mmu_enabled)
		vnode->alloc_ctx = vb2_dma_sg_init_ctx(&vnode->vdev.dev);
	else
		vnode->alloc_ctx = vb2_dma_contig_init_ctx(&vnode->vdev.dev);

	if (IS_ERR(vnode->alloc_ctx)) {
		ret = PTR_ERR(vnode->alloc_ctx);
		goto exit;
	}

	BLOCKING_INIT_NOTIFIER_HEAD(&vnode->notifier.head);

	/* Allow video device virtually act as DMA capable device */
	vnode->vdev.dev.coherent_dma_mask = DMA_BIT_MASK(32);
	vnode->vdev.dev.dma_mask = &vnode->vdev.dev.coherent_dma_mask;
	vnode->format.fmt.pix_mp = default_output_format;
	v4l2_ctrl_handler_init(&vnode->ctrl_handler, 0);
exit:
	return ret;
}
EXPORT_SYMBOL(isp_vnode_add);

void isp_vnode_remove(struct isp_vnode *vnode)
{
	if (video_is_registered(&vnode->vdev)) {
		if (vnode->mmu_enabled)
			vb2_dma_sg_cleanup_ctx(&vnode->alloc_ctx);
		else
			vb2_dma_contig_cleanup_ctx(&vnode->alloc_ctx);

		media_entity_cleanup(&vnode->vdev.entity);
		video_unregister_device(&vnode->vdev);
		v4l2_ctrl_handler_free(&vnode->ctrl_handler);
	}
}
EXPORT_SYMBOL(isp_vnode_remove);
