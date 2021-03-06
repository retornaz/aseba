#ifndef VPL_BLOCK_HOLDER_H
#define VPL_BLOCK_HOLDER_H

#include <QGraphicsItem>

namespace Aseba { namespace ThymioVPL
{
	/** \addtogroup studio */
	/*@{*/
	
	class Block;
	
	class BlockHolder : public QGraphicsObject
	{
		Q_OBJECT
		
	public:
		// constructor
		BlockHolder(const QString& blockType, QGraphicsItem *parent=0, Block* block=0);
		
		// from QGraphicsObject
		virtual QRectF boundingRect() const;
		
		// specific
		void addBlock(Block* block);
		void removeBlock(bool cleanupSet=true);
		const Block* getBlock() const { return block; }
		Block* getBlock() { return block; }
		
		void setAdvanced(bool advanced);
		bool isAnyAdvancedFeature() const;
		bool isAnyValueSet() const;
		bool isEmpty() const;
		
	signals:
		void contentChanged();
		void undoCheckpoint();
		
	protected:
		// from QGraphicsObject
		virtual void dragEnterEvent(QGraphicsSceneDragDropEvent *event);
		virtual void dragLeaveEvent(QGraphicsSceneDragDropEvent *event);
		virtual void dropEvent(QGraphicsSceneDragDropEvent *event);
		
		virtual void paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget);
		
		// specific
		bool isDnDValid(QGraphicsSceneDragDropEvent *event) const;
	
	protected:
		const QString blockType;
		bool highlight;
		Block* block;
	};
	
	/*@}*/
} } // namespace ThymioVPL / namespace Aseba

#endif
